// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "InferenceWorkload.h"
#include "NNEModelData.h"
#include "NNE.h"

namespace
{
template <typename T> bool CheckAllZero(const void* RawData, SIZE_T SizeInBytes)
{
	const uint32 NumElements = int32(SizeInBytes / sizeof(T));
	const T* TypedData = reinterpret_cast<const T*>(RawData);

	for (uint32 i = 0; i < NumElements; ++i)
	{
		if (TypedData[i] != T(0))
		{
			return false;
		}
	}
	return true;
}

template <typename T> bool ContainsZero(const void* RawData, SIZE_T SizeInBytes)
{
	const uint32 NumElements = int32(SizeInBytes / sizeof(T));
	const T* TypedData = reinterpret_cast<const T*>(RawData);

	for (uint32 i = 0; i < NumElements; ++i)
	{
		if (TypedData[i] == T(0))
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Output tensor contains zero value at index %d"), i);
			return true;
		}
	}
	return false;
}
} // namespace

Thespeon::Inference::InferenceWorkload::InferenceWorkload(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType)
{
	Backend = BackendType;
	SetModelinstance(ModelData, BackendType);
}

// NNE thread safety: IModelCPU::CreateModelInstanceCPU() / IModelGPU::CreateModelInstanceGPU()
// are safe to call concurrently from background threads. Verified empirically:
// TUNR-134 FPreloadSession dispatches parallel TaskGraph tasks that call RegisterModuleWorkload
// (which creates instances via the same IModel) — no crashes observed in Phase 3 concurrent
// preload tests (T4_MultiComponentConcurrentPreload, T6_PreloadThenImmediateSynthesis).
// The underlying ONNX Runtime session creation is documented as thread-safe.
TSharedPtr<Thespeon::Inference::InferenceWorkload, ESPMode::ThreadSafe> Thespeon::Inference::InferenceWorkload::CreateFromModel(
    TSharedPtr<UE::NNE::IModelCPU> InModelCPU, TSharedPtr<UE::NNE::IModelGPU> InModelGPU, EBackendType InBackend
)
{
	// Allocate via raw new since the default constructor is private
	TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> Workload(new InferenceWorkload());
	Workload->Backend = InBackend;
	Workload->SourceModelCPU = InModelCPU;
	Workload->SourceModelGPU = InModelGPU;

	switch (InBackend)
	{
		case EBackendType::CPU:
		case EBackendType::None:
		{
			if (!InModelCPU.IsValid())
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Error, TEXT("No source CPU model provided"));
				return nullptr;
			}
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateModelInstanceCPU");
			Workload->ModelInstance = InModelCPU->CreateModelInstanceCPU();
			break;
		}
		case EBackendType::GPU:
		{
			if (!InModelGPU.IsValid())
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Error, TEXT("No source GPU model provided"));
				return nullptr;
			}
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateModelInstanceGPU");
			Workload->ModelInstance = InModelGPU->CreateModelInstanceGPU();
			break;
		}
		default:
			LINGO_LOG_FUNC(EVerbosityLevel::Error, TEXT("Unsupported backend type"));
			return nullptr;
	}

	if (!Workload->ModelInstance.IsValid())
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Error, TEXT("Failed to create model instance"));
		return nullptr;
	}

	return Workload;
}

bool Thespeon::Inference::InferenceWorkload::SetCPU(const TStrongObjectPtr<UNNEModelData>& ModelData)
{
	using namespace UE::NNE;
	TWeakInterfacePtr<INNERuntimeCPU> Runtime = GetRuntime<INNERuntimeCPU>(FString("NNERuntimeORTCpu"));
	if (!Runtime.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Cannot find runtime NNERuntimeORTCpu, please enable the corresponding plugin"));
		return false;
	}

	SourceModelCPU = Runtime->CreateModelCPU(TObjectPtr<UNNEModelData>(ModelData.Get()));
	if (!SourceModelCPU.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to create model"));
		return false;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateModelInstanceCPU");
		ModelInstance = SourceModelCPU->CreateModelInstanceCPU();
	}

	return true;
}

bool Thespeon::Inference::InferenceWorkload::SetGPU(const TStrongObjectPtr<UNNEModelData>& ModelData)
{
	using namespace UE::NNE;
	TWeakInterfacePtr<INNERuntimeGPU> Runtime = GetRuntime<INNERuntimeGPU>(FString("NNERuntimeORTDml"));
	if (!Runtime.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Cannot find runtime NNERuntimeORTDml, please enable the corresponding plugin"));
		return false;
	}
	SourceModelGPU = Runtime->CreateModelGPU(TObjectPtr<UNNEModelData>(ModelData.Get()));
	if (!SourceModelGPU.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to create model"));
		return false;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateModelInstanceGPU");
		ModelInstance = SourceModelGPU->CreateModelInstanceGPU();
	}

	return true;
}

void Thespeon::Inference::InferenceWorkload::SetModelinstance(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType)
{
	if (!ModelData.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Inference Model Data is not loaded, please check the static path to your asset"));
		return;
	}

	switch (BackendType)
	{
		case EBackendType::CPU:
		// TODO in TUNR-135 remove None when default is handled by plugin settings
		case EBackendType::None:
			if (!SetCPU(ModelData))
			{
				return;
			}
			break;

		case EBackendType::GPU:
			if (!SetGPU(ModelData))
			{
				return;
			}
			break;

		default:
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Unsupported backend type"));
			return;
	}

	if (!ModelInstance.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to create model instance"));
		return;
	}
}

bool Thespeon::Inference::InferenceWorkload::Infer(
    SessionTensorPool& TensorPool, const TConstArrayView<UE::NNE::FTensorShape>& OutputTensorShapes, const FInferenceConfig& Config, FString debugName
)
{
	TArray<UE::NNE::FTensorBindingCPU> InputBindings, OutputBindings;
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*debugName);
	// 1) Prepare input bindings
	if (!ModelInstance.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("ModelInstance is not valid in '%s'"), *debugName);
		return false;
	}
	TConstArrayView<UE::NNE::FTensorDesc> InputDescs = ModelInstance->GetInputTensorDescs();
	InputBindings.SetNum(InputDescs.Num());
	TArray<UE::NNE::FTensorShape> InputShapes;
	InputShapes.Reserve(InputDescs.Num());

	for (int i = 0; i < InputDescs.Num(); ++i)
	{
		Thespeon::Inference::ModelIOData* Tensor = nullptr; // <-- raw pointer
		if (!TensorPool.TryGetTensor(InputDescs[i].GetName(), Tensor))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Input tensor '%s' has no data in '%s'"), *InputDescs[i].GetName(), *debugName);
			return false;
		}

		InputShapes.Add(Tensor->GetTensorShape());
		InputBindings[i].Data = Tensor->GetData();
		InputBindings[i].SizeInBytes = Tensor->GetMaxSizeInBytes();
	}
	ModelInstance->SetInputTensorShapes(InputShapes);

	// 2) Prepare output bindings
	TConstArrayView<UE::NNE::FTensorDesc> OutputDescs = ModelInstance->GetOutputTensorDescs();
	OutputBindings.SetNum(OutputDescs.Num());
	TArray<ModelIOData> OutputModelData;
	for (int i = 0; i < OutputDescs.Num(); ++i)
	{
		ModelIOData current = ModelIOData::Make(OutputTensorShapes[i], OutputDescs[i].GetDataType());
		OutputModelData.Add(MoveTemp(current));

		OutputBindings[i].Data = OutputModelData[i].GetData();
		OutputBindings[i].SizeInBytes = OutputModelData[i].GetMaxSizeInBytes();
	}

	// 3) Run the model
	auto status = ModelInstance->RunSync(InputBindings, OutputBindings);

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Model run status: %d"), status);

	if (status == UE::NNE::IModelInstanceRunSync::ERunSyncStatus::Fail)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Model run failed"));
		return false;
	}

	// Debug during development only - checks that no output tensor is all zero
	// Skip validation for phonemizer as it's expected to have zero tensors (new_finished_indices, num_finished)
	if (!debugName.Equals(TEXT("Phonemizer")) && !ValidateOutputs(OutputModelData))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Some tensor is all zero at %s. Either its ok or output was underallocated."), *debugName);
	}
	for (int i = 0; i < OutputDescs.Num(); ++i)
	{
		TensorPool.SetTensor(OutputDescs[i].GetName(), MoveTemp(OutputModelData[i]));
	}
	return true;
}

TConstArrayView<UE::NNE::FTensorDesc> Thespeon::Inference::InferenceWorkload::GetOutputTensorDesc()
{
	if (!ModelInstance.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("GetOutputTensorDesc called with invalid ModelInstance"));
		return {};
	}
	return ModelInstance->GetOutputTensorDescs();
}

SIZE_T Thespeon::Inference::InferenceWorkload::CalculateByteSize(const UE::NNE::FTensorShape& Shape, uint32 ByteSize)
{
	uint64 count = 1;
	for (uint32 d : Shape.GetData())
	{
		check(d > 0);
		count *= d;
	}

	return SIZE_T(count) * ByteSize;
}

// checks if any tensor is all zero - useful to catch underallocation of output tensors.
bool Thespeon::Inference::InferenceWorkload::ValidateOutputs(const TArray<ModelIOData>& ModelIO)
{
	TArray<bool> AllZeroTensors;
	for (const ModelIOData& Data : ModelIO)
	{
		bool allZero = true;
		switch (Data.GetTensorDataType())
		{
			case ENNETensorDataType::Float:
				allZero = CheckAllZero<float>(Data.GetData(), Data.GetMaxSizeInBytes());
				break;
			case ENNETensorDataType::Int32:
				allZero = CheckAllZero<int32>(Data.GetData(), Data.GetMaxSizeInBytes());
				break;
			case ENNETensorDataType::Int64:
				allZero = CheckAllZero<int64>(Data.GetData(), Data.GetMaxSizeInBytes());
				break;
			case ENNETensorDataType::UInt8:
				allZero = CheckAllZero<uint8>(Data.GetData(), Data.GetMaxSizeInBytes());
				break;
			default:
				LINGO_LOG(EVerbosityLevel::Warning, TEXT("Unsupported data type %d"), (int)Data.GetTensorDataType());
				return false;
		}
		AllZeroTensors.Add(allZero);
	}
	if (AllZeroTensors.Contains(true))
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("One or more output tensors are all zero: %s\n There is a risk that you underallocated output tensors."),
		    *FString::JoinBy(AllZeroTensors, TEXT(", "), [](bool v) { return v ? TEXT("true") : TEXT("false"); })
		);
		return false;
	}

	return true;
}

bool CheckTensorForZeros(const Thespeon::Inference::ModelIOData& Data)
{
	switch (Data.GetTensorDataType())
	{
		case ENNETensorDataType::Float:
			return ContainsZero<float>(Data.GetData(), Data.GetMaxSizeInBytes());
		case ENNETensorDataType::Int32:
			return ContainsZero<int32>(Data.GetData(), Data.GetMaxSizeInBytes());
		case ENNETensorDataType::Int64:
			return ContainsZero<int64>(Data.GetData(), Data.GetMaxSizeInBytes());
		case ENNETensorDataType::UInt8:
			return ContainsZero<uint8>(Data.GetData(), Data.GetMaxSizeInBytes());
		default:
			return false;
	}
}

// Debugging only - can be used to find blocks of zeros in mel tensors - a known reason for clicks in audio.
bool Thespeon::Inference::InferenceWorkload::AnyZero(const TArray<ModelIOData>& ModelIO)
{
	for (const auto& Data : ModelIO)
	{
		if (CheckTensorForZeros(Data))
		{
			return true;
		}
	}
	return false;
}

// use to print mels to console in a nicely formatted way for debugging
FString Thespeon::Inference::InferenceWorkload::FormatMel(const ModelIOData& MelData)
{
	if (MelData.GetTensorDataType() != ENNETensorDataType::Float)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("FormatMel: Data is not float"));
		return TEXT("<not float data>");
	}
	if (MelData.GetTensorShape().GetData()[1] != 96)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("FormatMel: Data is not mel"));
		return TEXT("<not mel data>");
	}
	const int32 NumFloats = int32(MelData.GetMaxSizeInBytes() / sizeof(float));
	const float* FloatData = reinterpret_cast<const float*>(MelData.GetData());
	FString Result;
	for (int32 i = 0; i < NumFloats; ++i)
	{
		Result += FString::Printf(TEXT("%.1f"), FloatData[i]);
		if (i < NumFloats - 1)
		{
			Result += TEXT(", ");
		}
		if ((i + 1) % MelData.GetTensorShape().GetData()[2] == 0)
		{
			Result += TEXT("\n");
		}
	}
	return Result;
}

// For debugging. Gets tensor data as a comma-separated string.
FString Thespeon::Inference::InferenceWorkload::GetDataAsString(const ModelIOData& Data)
{
	switch (Data.GetTensorDataType())
	{
		case ENNETensorDataType::Float:
		{
			const int32 NumFloats = int32(Data.GetMaxSizeInBytes() / sizeof(float));
			const float* FloatData = reinterpret_cast<const float*>(Data.GetData());
			return FString::JoinBy(TArrayView<const float>(FloatData, NumFloats), TEXT(", "), [](float v) { return FString::SanitizeFloat(v); });
		}
		case ENNETensorDataType::Int32:
		{
			const int32 NumInts = int32(Data.GetMaxSizeInBytes() / sizeof(int32));
			const int32* IntData = reinterpret_cast<const int32*>(Data.GetData());
			return FString::JoinBy(TArrayView<const int32>(IntData, NumInts), TEXT(", "), [](int32 v) { return FString::FromInt(v); });
		}
		case ENNETensorDataType::Int64:
		{
			const int32 NumInts = int32(Data.GetMaxSizeInBytes() / sizeof(int64));
			const int64* IntData = reinterpret_cast<const int64*>(Data.GetData());
			return FString::JoinBy(
			    TArrayView<const int64>(IntData, NumInts), TEXT(", "), [](int64 v) { return FString::Printf(TEXT("%lld"), (long long)v); }
			);
		}
		case ENNETensorDataType::UInt8:
		{
			const int32 NumBytes = int32(Data.GetMaxSizeInBytes() / sizeof(uint8));
			const uint8* ByteData = reinterpret_cast<const uint8*>(Data.GetData());
			return FString::JoinBy(TArrayView<const uint8>(ByteData, NumBytes), TEXT(", "), [](uint8 v) { return FString::FromInt(v); });
		}
		default:
			return FString::Printf(TEXT("<unsupported data type %d>"), (int)Data.GetTensorDataType());
	}
}
