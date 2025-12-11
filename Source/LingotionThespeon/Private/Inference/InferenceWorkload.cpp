// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "InferenceWorkload.h"
#include "NNEModelData.h"
#include "NNE.h"

Thespeon::Inference::InferenceWorkload::InferenceWorkload(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType)
{
    using namespace UE::NNE;
    if (!ModelData.IsValid())
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Inference Model Data is not loaded, please check the static path to your asset"));
        return;
	}
    if(BackendType == EBackendType::CPU)
    {
        TWeakInterfacePtr<INNERuntimeCPU> Runtime = GetRuntime<INNERuntimeCPU>(FString("NNERuntimeORTCpu"));
        if (!Runtime.IsValid())
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Cannot find runtime NNERuntimeORTCpu, please enable the corresponding plugin"));
            return;
        }

        TSharedPtr<UE::NNE::IModelCPU> Model = Runtime->CreateModelCPU(TObjectPtr<UNNEModelData>(ModelData.Get()));
        if (!Model.IsValid())
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to create model"));
            return;
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateModelInstanceCPU");
            ModelInstance = Model->CreateModelInstanceCPU();
        }
    } else if (BackendType == EBackendType::GPU)
    {
        TWeakInterfacePtr<INNERuntimeGPU> Runtime = GetRuntime<INNERuntimeGPU>(FString("NNERuntimeORTDml"));
        if (!Runtime.IsValid())
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Cannot find runtime NNERuntimeORTCpu, please enable the corresponding plugin"));
            return;
        }
        TSharedPtr<UE::NNE::IModelGPU> Model = Runtime->CreateModelGPU(TObjectPtr<UNNEModelData>(ModelData.Get()));
        if (!Model.IsValid())
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to create model"));
            return;
        }
        TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateModelInstanceGPU");
        ModelInstance = Model->CreateModelInstanceGPU();
    } else 
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Unsupported backend type"));
        return;
    }
    if(!ModelInstance.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to create model instance"));
        return;
    }
}

void Thespeon::Inference::InferenceWorkload::Infer(SessionTensorPool& TensorPool, const TConstArrayView<UE::NNE::FTensorShape>& OutputTensorShapes, const FInferenceConfig& Config, FString debugName)
{
    TArray<UE::NNE::FTensorBindingCPU> InputBindings, OutputBindings;
    TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*debugName);
    // 1) Prepare input bindings
    if(ModelInstance == nullptr || !ModelInstance.IsValid())
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("ModelInstance is not valid in '%s'"), *debugName);
        return;
    }
    TConstArrayView<UE::NNE::FTensorDesc> InputDescs = ModelInstance->GetInputTensorDescs();
    InputBindings.SetNum(InputDescs.Num());
    TArray<UE::NNE::FTensorShape> InputShapes; 
    InputShapes.Reserve(InputDescs.Num());
    
    for (int i = 0; i < InputDescs.Num(); ++i)
    {
        Thespeon::Inference::ModelIOData* Tensor = nullptr;   // <-- raw pointer
        if (!TensorPool.TryGetTensor(InputDescs[i].GetName(), Tensor))
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Input tensor '%s' has no data in '%s'"),
                *InputDescs[i].GetName(), *debugName);
            return;
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
    for(int i = 0; i < OutputDescs.Num(); ++i)
    {   
        ModelIOData current = ModelIOData::Make(OutputTensorShapes[i], OutputDescs[i].GetDataType());
        OutputModelData.Add(MoveTemp(current)); 
        
        OutputBindings[i].Data        = OutputModelData[i].GetData();
        OutputBindings[i].SizeInBytes = OutputModelData[i].GetMaxSizeInBytes();
    }

    // 3) Run the model
    auto status = ModelInstance->RunSync(InputBindings, OutputBindings);

    LINGO_LOG(EVerbosityLevel::Debug, TEXT("Model run status: %d"), status);

    if (status == UE::NNE::IModelInstanceRunSync::ERunSyncStatus::Fail)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Model run failed"));
    }

    // Debug during development only - checks that no output tensor is all zero
    // Skip validation for phonemizer as it's expected to have zero tensors (new_finished_indices, num_finished)
    if (!debugName.Equals(TEXT("Phonemizer")) && !ValidateOutputs(OutputModelData))
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Some tensor is all zero at %s. Either its ok or output was underallocated."), *debugName);
    }
    for (int i = 0; i < OutputDescs.Num(); ++i)
    {
        TensorPool.SetTensor(OutputDescs[i].GetName(), MoveTemp(OutputModelData[i]));
    }
}

TConstArrayView<UE::NNE::FTensorShape> Thespeon::Inference::InferenceWorkload::MockInfer(SessionTensorPool& TensorPool, const FInferenceConfig& Config, FString debugName)
{
    TArray<UE::NNE::FTensorBindingCPU> InputBindings, OutputBindings;
    TConstArrayView<UE::NNE::FTensorShape> result;
    // 1) Prepare input bindings
    TConstArrayView<UE::NNE::FTensorDesc> InputDescs = ModelInstance->GetInputTensorDescs();
    InputBindings.SetNum(InputDescs.Num());
    TArray<UE::NNE::FTensorShape> InputShapes; 
    InputShapes.Reserve(InputDescs.Num());
    
    for (int i = 0; i < InputDescs.Num(); ++i)
    {
        Thespeon::Inference::ModelIOData* Tensor = nullptr;
        if (!TensorPool.TryGetTensor(InputDescs[i].GetName(), Tensor))
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Input tensor '%s' has no data in '%s'"),
                *InputDescs[i].GetName(), *debugName);
            return result;
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
    for(int i = 0; i < OutputDescs.Num(); ++i)
    {   
        ModelIOData current = ModelIOData::Make(UE::NNE::FTensorShape::Make({ 1, 1, 1, 1}), OutputDescs[i].GetDataType());
        OutputModelData.Add(MoveTemp(current)); 
        
        OutputBindings[i].Data        = OutputModelData[i].GetData();
        OutputBindings[i].SizeInBytes = OutputModelData[i].GetMaxSizeInBytes();
    }

    // 3) Run the model
    auto status = ModelInstance->RunSync(InputBindings, OutputBindings);

    LINGO_LOG(EVerbosityLevel::Debug, TEXT("Model run status: %d"), status);

    if (status == UE::NNE::IModelInstanceRunSync::ERunSyncStatus::Fail)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Model run failed"));
    }

    result = ModelInstance->GetOutputTensorShapes();
    return result;
}

TConstArrayView<UE::NNE::FTensorDesc> Thespeon::Inference::InferenceWorkload::GetOutputTensorDesc()
{
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
            {
                const int32 NumFloats = int32(Data.GetMaxSizeInBytes() / sizeof(float));
                const float* FloatData = reinterpret_cast<const float*>(Data.GetData());
                for (int32 i = 0; i < NumFloats; ++i)
                {
                    if (FloatData[i] != 0.0f)
                    {
                        allZero = false;
                        break;
                    }
                }
                break;
            }
            case ENNETensorDataType::Int32:
            {
                const int32 NumInts = int32(Data.GetMaxSizeInBytes() / sizeof(int32));
                const int32* IntData = reinterpret_cast<const int32*>(Data.GetData());
                for (int32 i = 0; i < NumInts; ++i)
                {
                    if (IntData[i] != 0)
                    {
                        allZero = false;
                        break;
                    }
                }
                break;
            }
            case ENNETensorDataType::Int64:
            {
                const int32 NumInts = int32(Data.GetMaxSizeInBytes() / sizeof(int64));
                const int64* IntData = reinterpret_cast<const int64*>(Data.GetData());
                for (int32 i = 0; i < NumInts; ++i)
                {
                    if (IntData[i] != 0)
                    {
                        allZero = false;
                        break;
                    }
                }
                break;
            }
            case ENNETensorDataType::UInt8:
            {
                const int32 NumBytes = int32(Data.GetMaxSizeInBytes() / sizeof(uint8));
                const uint8* ByteData = reinterpret_cast<const uint8*>(Data.GetData());
                for (int32 i = 0; i < NumBytes; ++i)
                {
                    if (ByteData[i] != 0)
                    {
                        allZero = false;
                        break;
                    }
                }
                break;
            }
            default:
                LINGO_LOG(EVerbosityLevel::Warning, TEXT("Unsupported data type %d"), (int)Data.GetTensorDataType());
                return false;
        }
        AllZeroTensors.Add(allZero);
    }
    if(AllZeroTensors.Contains(true))
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("One or more output tensors are all zero: %s\n There is a risk that you underallocated output tensors."), *FString::JoinBy(AllZeroTensors, TEXT(", "), [](bool v) { return v ? TEXT("true") : TEXT("false"); }));
        return false;
    }
    
    return true;
}

//Debugging only - can be used to find blocks of zeros in mel tensors - a known reason for clicks in audio.
bool Thespeon::Inference::InferenceWorkload::AnyZero(const TArray<ModelIOData>& ModelIO)
{
    for (const auto& Data : ModelIO)
    {
        switch (Data.GetTensorDataType())
        {
            case ENNETensorDataType::Float:
            {
                const int32 NumFloats = int32(Data.GetMaxSizeInBytes() / sizeof(float));
                const float* FloatData = reinterpret_cast<const float*>(Data.GetData());
                for (int32 i = 0; i < NumFloats; ++i)
                {
                    if (FloatData[i] == 0.0f)
                    {
                        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Output tensor contains zero float value at index %d"), i);
                        return true;
                    }
                }
                break;
            }
            case ENNETensorDataType::Int32:
            {
                const int32 NumInts = int32(Data.GetMaxSizeInBytes() / sizeof(int32));
                const int32* IntData = reinterpret_cast<const int32*>(Data.GetData());
                for (int32 i = 0; i < NumInts; ++i)
                {
                    if (IntData[i] == 0)
                    {
                        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Output tensor contains zero int32 value at index %d"), i);
                        return true;
                    }
                }
                break;
            }
            case ENNETensorDataType::Int64:
            {
                const int32 NumInts = int32(Data.GetMaxSizeInBytes() / sizeof(int64));
                const int64* IntData = reinterpret_cast<const int64*>(Data.GetData());
                for (int32 i = 0; i < NumInts; ++i)
                {
                    if (IntData[i] == 0)
                    {
                        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Output tensor contains zero int64 value at index %d"), i);
                        return true;
                    }
                }
                break;
            }
            case ENNETensorDataType::UInt8:
            {
                const int32 NumBytes = int32(Data.GetMaxSizeInBytes() / sizeof(uint8));
                const uint8* ByteData = reinterpret_cast<const uint8*>(Data.GetData());
                for (int32 i = 0; i < NumBytes; ++i)
                {
                    if (ByteData[i] == 0)
                    {
                        LINGO_LOG(EVerbosityLevel::Debug, TEXT("Output tensor contains zero uint8 value at index %d"), i);
                        return true;
                    }
                }
                break;
            }
        }
    }
    return false;
}

//use to print mels to console in a nicely formatted way for debugging
FString Thespeon::Inference::InferenceWorkload::FormatMel(const ModelIOData& MelData)
{
    if (MelData.GetTensorDataType() != ENNETensorDataType::Float)
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("FormatMel: Data is not float"));
        return TEXT("<not float data>");
    }
    if (MelData.GetTensorShape().GetData()[1] != 96)
    {
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("FormatMel: Data is not mel"));
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
            return FString::JoinBy(TArrayView<const int64>(IntData, NumInts), TEXT(", "), [](int64 v) { return FString::Printf(TEXT("%lld"), (long long)v); });
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
