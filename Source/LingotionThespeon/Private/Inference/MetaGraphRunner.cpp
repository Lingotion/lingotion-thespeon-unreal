#include "MetaGraphRunner.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Character/CharacterModule.h"
#include "Utils/WavSaver.h"
#include "SymExprParser.h"

using namespace Thespeon::Inference;

FMetaGraphRunner::FMetaGraphRunner(
    SessionTensorPool& InTensorPool,
    Thespeon::Character::CharacterModule* InCharacterModule,
    Thespeon::Inference::FSessionWorkloadCache* InWorkloadCache,
    const FInferenceConfig& InConfig,
    Thespeon::Inference::FPostPacketFn InCallback,
    Thespeon::Inference::FShouldStopFn InStopSignal
)
    : TensorPool(InTensorPool)
    , WorkloadCache(InWorkloadCache)
    , CharacterModule(InCharacterModule)
    , Config(InConfig)
    , PostPacketCallback(MoveTemp(InCallback))
    , ExternStopSignal(MoveTemp(InStopSignal))
{
}

static FString ToFString(const std::string& S)
{
	return FString(S.c_str());
}

// convert proto string -> FString
static FString ToFString(const std::string& S, const TCHAR* /*Tag*/)
{
	return FString(UTF8_TO_TCHAR(S.c_str()));
}

float FMetaGraphRunner::HostToFloat(const FHostValue& V)
{
	if (const int64* I = V.TryGet<int64>())
	{
		return static_cast<float>(*I);
	}
	if (const float* F = V.TryGet<float>())
	{
		return *F;
	}
	if (const bool* B = V.TryGet<bool>())
	{
		return *B ? 1.0f : 0.0f;
	}

	if (const ModelIOData* M = V.TryGet<ModelIOData>())
	{
		switch (M->GetTensorDataType())
		{
			case ENNETensorDataType::Float:
				return M->GetDataAsValue<float>(0);
			case ENNETensorDataType::Int32:
				return static_cast<float>(M->GetDataAsValue<int32>(0));
			case ENNETensorDataType::Int64:
				return static_cast<float>(M->GetDataAsValue<int64>(0));
			default:
				break;
		}
	}

	checkNoEntry();
	return 0.0f;
}

bool FMetaGraphRunner::HostToBool(const FHostValue& V)
{
	if (const bool* B = V.TryGet<bool>())
	{
		return *B;
	}
	if (const int64* I = V.TryGet<int64>())
	{
		return (*I != 0);
	}
	if (const float* F = V.TryGet<float>())
	{
		return (*F != 0.0f);
	}

	checkNoEntry();
	return false;
}

int64 FMetaGraphRunner::NumElements(const TArray<int64>& Dims)
{
	int64 N = 1;
	for (int64 D : Dims)
	{
		N *= D;
	}
	return N;
}

UE::NNE::FTensorShape FMetaGraphRunner::MakeShapeFromDims(const TArray<int64>& Dims)
{
	TArray<uint32> IntDims;
	IntDims.Reserve(Dims.Num());
	for (int64 D : Dims)
	{
		IntDims.Add(static_cast<uint32>(D));
	}
	return UE::NNE::FTensorShape::Make(IntDims);
}

TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> FMetaGraphRunner::ResolveWorkloadForNode(const metaonnx::Node& Node) const
{
	if (!WorkloadCache || !CharacterModule)
	{
		return nullptr;
	}

	const FString LogicalName = ToFString(Node.id());
	auto InternalID = CharacterModule->GetInternalModelID(*LogicalName);
	return WorkloadCache->GetWorkload(InternalID);
}

// Convert ScalarLiteral -> FHostValue.
bool FMetaGraphRunner::ScalarLiteralToHostValue(const metaonnx::ScalarLiteral& Lit, FHostValue& Out)
{
	using Case = metaonnx::ScalarLiteral::ValueCase;
	switch (Lit.value_case())
	{
		case Case::kI64:
			Out.Set<int64>(static_cast<int64>(Lit.i64()));
			return true;
		case Case::kF32:
			Out.Set<float>(static_cast<float>(Lit.f32()));
			return true;
		case Case::kB:
			Out.Set<bool>(static_cast<bool>(Lit.b()));
			return true;
		case Case::kS:
			Out.Set<FString>(ToFString(Lit.s(), TEXT("ScalarLiteral.s")));
			return true;
		default:
			return false;
	}
}

// Resolve ValueRef into a host-side value.
bool FMetaGraphRunner::ResolveValueRef(
    const metaonnx::ValueRef& Ref, const Thespeon::Inference::SessionTensorPool& TensorPool, const FHostMap& Host, FHostValue& Out
)
{
	using Kind = metaonnx::ValueRef::KindCase;

	switch (Ref.kind_case())
	{
		case Kind::kHost:
		{
			const FString Name = ToFString(Ref.host().name(), TEXT("ValueRef.host.name"));
			if (const FHostValue* HV = Host.Find(Name))
			{
				Out = *HV;
				return true;
			}
			LINGO_LOG(EVerbosityLevel::Error, TEXT("ValueRef(host): missing host var %s"), *Name);
			return false;
		}

		case Kind::kLiteral:
		{
			if (!ScalarLiteralToHostValue(Ref.literal(), Out))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("ValueRef(literal): missing/invalid scalar literal"));
				return false;
			}
			return true;
		}

		case Kind::kTensor:
		{
			const FString Name = ToFString(Ref.tensor().name(), TEXT("ValueRef.tensor.name"));
			const ModelIOData* T = nullptr;
			if (!TensorPool.TryGetTensor(Name, T) || !T)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("ValueRef(tensor): missing tensor %s"), *Name);
				return false;
			}

			if (Ref.tensor().has_dim())
			{
				const int64 DimIndex = static_cast<int64>(Ref.tensor().dim());
				const auto& Shape = T->GetTensorShape();

				const TConstArrayView<uint32> DataView = Shape.GetData();
				if (!DataView.IsValidIndex(static_cast<int32>(DimIndex)))
				{
					LINGO_LOG(EVerbosityLevel::Error, TEXT("ValueRef(tensor): dim %lld out of range for %s"), DimIndex, *Name);
					return false;
				}

				Out.Set<int64>(static_cast<int64>(DataView[static_cast<int32>(DimIndex)]));
				return true;
			}

			Out.Set<ModelIOData>(*T);
			return true;
		}

		default:
			LINGO_LOG(EVerbosityLevel::Error, TEXT("ValueRef: kind not set"));
			return false;
	}
}

// Evaluates a host-side action from the meta graph. Supports: tensor<->host transfers,
// tensor copy/rename/create, host variable set/remove, binary arithmetic, and callbacks
// (which post audio/trigger data packets back to the game thread).
void FMetaGraphRunner::EvalHostAction(const metaonnx::HostAction& Action, FHostMap& Host)
{
	using Case = metaonnx::HostAction::ActionCase;

	switch (Action.action_case())
	{
		case Case::kTensorToHost:
		{
			const auto& A = Action.tensor_to_host();
			const FString Dest = ToFString(A.dest_host());
			const FString Src = ToFString(A.src_tensor().name());

			ModelIOData* SrcTensor = nullptr;
			if (!TensorPool.TryGetTensor(Src, SrcTensor) || !SrcTensor)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("TensorToHost: missing tensor %s"), *Src);
				return;
			}

			FHostValue ResultValue;
			ResultValue.Set<ModelIOData>(*SrcTensor);
			Host.Add(Dest, ResultValue);
			return;
		}

		case Case::kHostToTensor:
		{
			const auto& A = Action.host_to_tensor();
			const FString Dest = ToFString(A.dest_tensor().name());
			const FString Src = ToFString(A.src_host());

			FHostValue* SrcVal = Host.Find(Src);
			if (!SrcVal)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("HostToTensor: missing host var %s"), *Src);
				return;
			}

			TUniquePtr<ModelIOData> NewTensor = nullptr;

			if (int64* I = SrcVal->TryGet<int64>())
			{
				TArray<int64> Arr{*I};
				NewTensor = MakeUnique<ModelIOData>(ModelIOData::MakeFromArray<int64>(UE::NNE::FTensorShape::Make({1}), Arr));
			}
			else if (float* F = SrcVal->TryGet<float>())
			{
				TArray<float> Arr{*F};
				NewTensor = MakeUnique<ModelIOData>(ModelIOData::MakeFromArray<float>(UE::NNE::FTensorShape::Make({1}), Arr));
			}
			else if (bool* B = SrcVal->TryGet<bool>())
			{
				TArray<int64> Arr{*B ? 1 : 0};
				NewTensor = MakeUnique<ModelIOData>(ModelIOData::MakeFromArray<int64>(UE::NNE::FTensorShape::Make({1}), Arr));
			}
			else
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("HostToTensor: unsupported host type for %s"), *Src);
				return;
			}

			TensorPool.SetTensor(Dest, MoveTemp(NewTensor));
			return;
		}

		case Case::kTensorCopy:
		{
			const auto& A = Action.tensor_copy();
			const FString Dest = ToFString(A.dest_tensor().name());
			const FString Src = ToFString(A.src_tensor().name());

			ModelIOData* SrcTensor = nullptr;
			if (!TensorPool.TryGetTensor(Src, SrcTensor) || !SrcTensor)
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("TensorCopy: missing tensor %s"), *Src);
				return;
			}

			TensorPool.SetTensor(Dest, ModelIOData(*SrcTensor));
			return;
		}

		case Case::kTensorRename:
		{
			const auto& A = Action.tensor_rename();
			const FString Dest = ToFString(A.dest_tensor());
			const FString Src = ToFString(A.src_tensor());

			if (!TensorPool.TryRenameTensor(Src, Dest))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("TensorRename: failed %s -> %s"), *Src, *Dest);
			}
			return;
		}

		case Case::kCallback:
		{
			const auto& Cb = Action.callback();

			Thespeon::Core::FThespeonDataPacket ResultPacket(static_cast<Thespeon::Core::SynthCallbackType>(static_cast<uint8>(Cb.callback_type())));

			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug,
			    TEXT("HostCallback type=%d name=%s args=%d"),
			    static_cast<int32>(Cb.callback_type()),
			    *ToFString(Cb.callback_name()),
			    Cb.args_size()
			);

			switch (Cb.callback_type())
			{
				case metaonnx::CallbackType::CB_ERROR:
				{
					// In case we want to bake in error packet sending from inside the meta graph, like input too short-handling?
					break;
				}
				case metaonnx::CallbackType::CB_AUDIO:
				{
					TArray<float> ResultArr;
					FHostValue bIsFinal;
					// Iterate over arguments and append them to a result array
					for (int i = 0; i < Cb.args_size(); ++i)
					{
						FHostValue V;
						if (!ResolveValueRef(Cb.args(i), TensorPool, Host, V))
						{
							LINGO_LOG(EVerbosityLevel::Error, TEXT("HostCallback: failed to resolve arg %d"), i);
							return;
						}

						if (ModelIOData* TensorVal = V.TryGet<ModelIOData>())
						{
							ResultArr.Append(TensorVal->GetDataAsArray<float>());
						}
						else if (TArray<float>* ArrVal = V.TryGet<TArray<float>>())
						{
							ResultArr.Append(*ArrVal);
						}
						else
						{
							LINGO_LOG(EVerbosityLevel::Error, TEXT("HostCallback: Unsuitable target data for audio callback"));
						}
					}

					ResultPacket.Payload.Set<TArray<float>>(ResultArr);

					break;
				}
				case metaonnx::CallbackType::CB_TRIGGERSAMPLE:
				{
					TArray<int64> ResultArr;
					// Iterate over arguments and append them to a result array
					for (int i = 0; i < Cb.args_size(); ++i)
					{
						FHostValue V;
						if (!ResolveValueRef(Cb.args(i), TensorPool, Host, V))
						{
							LINGO_LOG(EVerbosityLevel::Error, TEXT("HostCallback: failed to resolve arg %d"), i);
							return;
						}

						if (ModelIOData* TensorVal = V.TryGet<ModelIOData>())
						{
							ResultArr.Append(TensorVal->GetDataAsArray<int64>());
						}
						else if (TArray<int64>* ArrVal = V.TryGet<TArray<int64>>())
						{
							ResultArr.Append(*ArrVal);
						}
						else
						{
							LINGO_LOG(EVerbosityLevel::Error, TEXT("HostCallback: Unsuitable target data for audio callback"));
						}
					}

					ResultPacket.Payload.Set<TArray<int64>>(ResultArr);
					break;
				}
				default:
				{

					LINGO_LOG(EVerbosityLevel::Error, TEXT("HostCallback: Unspecified callback type used."));
					return;
				}
			}

			// Iterate over metadata objects
			Thespeon::Core::FMetadataMap MetadataMap;
			for (int i = 0; i < Cb.metadata_size(); ++i)
			{
				FHostValue V;
				if (!ScalarLiteralToHostValue(Cb.metadata(i).value(), V))
				{
					LINGO_LOG(EVerbosityLevel::Error, TEXT("HostCallback: Unknown metadata value"));
				}
				Thespeon::Core::FPacketMetadataValue MetaVal;
				if (int64* IntVal = V.TryGet<int64>())
				{
					MetaVal.Set<int64>(*IntVal);
				}
				else if (float* FloatVal = V.TryGet<float>())
				{
					MetaVal.Set<float>(*FloatVal);
				}
				else if (bool* BoolVal = V.TryGet<bool>())
				{
					MetaVal.Set<bool>(*BoolVal);
				}
				else if (FString* StringVal = V.TryGet<FString>())
				{
					MetaVal.Set<FString>(*StringVal);
				}
				else
				{
					LINGO_LOG(EVerbosityLevel::Error, TEXT("HostCallback: Unsuitable target data for metadata"));
					continue;
				}
				FString Key = ToFString(Cb.metadata(i).key());

				LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("HostCallback: found metadata value [%s]"), *Key);
				MetadataMap.Add(Key, MetaVal);
			}
			ResultPacket.Metadata = MetadataMap;

			PostPacketCallback(ResultPacket);

			return;
		}

		case Case::kHostBinop:
		{
			const auto& A = Action.host_binop();
			const FString Dest = ToFString(A.dest_host());

			FHostValue LeftVal;
			FHostValue RightVal;

			if (!ResolveValueRef(A.left(), TensorPool, Host, LeftVal))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("HostBinaryOp: missing/invalid left"));
				return;
			}
			if (!ResolveValueRef(A.right(), TensorPool, Host, RightVal))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("HostBinaryOp: missing/invalid right"));
				return;
			}

			const float Left = HostToFloat(LeftVal);
			const float Right = HostToFloat(RightVal);

			float Result = 0.0f;
			switch (A.op())
			{
				case metaonnx::HostBinaryOp_Op_ADD:
					Result = Left + Right;
					break;
				case metaonnx::HostBinaryOp_Op_SUB:
					Result = Left - Right;
					break;
				case metaonnx::HostBinaryOp_Op_MUL:
					Result = Left * Right;
					break;
				case metaonnx::HostBinaryOp_Op_DIV:
					Result = Left / Right;
					break;
				case metaonnx::HostBinaryOp_Op_MOD:
					Result = static_cast<float>(static_cast<int32>(Left) % static_cast<int32>(Right));
					break;
				default:
					LINGO_LOG(EVerbosityLevel::Error, TEXT("HostBinaryOp: unknown op %d"), (int32)A.op());
					return;
			}

			FHostValue ResultVal;

			// Preserve previous behavior: keep float if left was float, else int64.
			if (LeftVal.IsType<float>())
			{
				ResultVal.Set<float>(Result);
			}
			else
			{
				ResultVal.Set<int64>(static_cast<int64>(Result));
			}

			Host.Add(Dest, ResultVal);
			return;
		}

		case Case::kTensorCreate:
		{
			const auto& A = Action.tensor_create();
			const FString Dest = ToFString(A.dest_tensor().name());

			TArray<int64> Dims;
			Dims.Reserve(A.dims_literal_size());
			for (int i = 0; i < A.dims_literal_size(); ++i)
			{
				Dims.Add(static_cast<int64>(A.dims_literal(i)));
			}
			if (Dims.Num() == 0)
			{
				Dims.Add(1);
			}

			const UE::NNE::FTensorShape Shape = MakeShapeFromDims(Dims);
			const std::string& DTypeStr = A.dtype();

			// Determine fill value
			const bool bOnes = (A.fill() == metaonnx::TensorCreate_Fill_ONES);
			const bool bZeros = (A.fill() == metaonnx::TensorCreate_Fill_ZEROS);
			const bool bValue = (A.fill() == metaonnx::TensorCreate_Fill_VALUE);

			if (!(bOnes || bZeros || bValue))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("TensorCreate: unknown fill %d"), (int32)A.fill());
				return;
			}

			FHostValue FillValue;
			if (bValue)
			{
				if (!A.has_value() || !ScalarLiteralToHostValue(A.value(), FillValue))
				{
					LINGO_LOG(EVerbosityLevel::Error, TEXT("TensorCreate: VALUE fill requires scalar value"));
					return;
				}
			}

			if (DTypeStr == "int64")
			{
				int64 Val = bOnes ? 1 : 0;
				if (bValue)
				{
					Val = static_cast<int64>(HostToFloat(FillValue));
				}
				TensorPool.SetTensor(Dest, MakeUnique<ModelIOData>(ModelIOData::Make<int64>(Shape, Val)));
			}
			else if (DTypeStr == "float32")
			{
				float Val = bOnes ? 1.0f : 0.0f;
				if (bValue)
				{
					Val = HostToFloat(FillValue);
				}
				TensorPool.SetTensor(Dest, MakeUnique<ModelIOData>(ModelIOData::Make<float>(Shape, Val)));
			}
			else
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("TensorCreate: unsupported dtype %s"), *FString(DTypeStr.c_str()));
				return;
			}

			return;
		}

		case Case::kHostSet:
		{
			const auto& A = Action.host_set();
			const FString Dest = ToFString(A.dest_host());

			FHostValue TargetValue;
			if (!ResolveValueRef(A.value(), TensorPool, Host, TargetValue))
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("HostSetVariable: missing target value"));
				return;
			}

			Host.Add(Dest, TargetValue);
			return;
		}

		case Case::kHostRemove:
		{
			const auto& A = Action.host_remove();
			const FString Target = ToFString(A.name());

			Host.Remove(Target);
			return;
		}

		default:
			LINGO_LOG(EVerbosityLevel::Error, TEXT("EvalHostAction: unknown/unset action_case"));
			return;
	}
}

bool FMetaGraphRunner::RunCondition(const metaonnx::Condition& Cond, FHostMap& Host) const
{
	FHostValue LeftVal;
	FHostValue RightVal;

	if (!ResolveValueRef(Cond.left(), TensorPool, Host, LeftVal))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Condition: failed to resolve left"));
		return false;
	}
	if (!ResolveValueRef(Cond.right(), TensorPool, Host, RightVal))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Condition: failed to resolve right"));
		return false;
	}

	const float Left = HostToFloat(LeftVal);
	const float Right = HostToFloat(RightVal);

	using Cmp = metaonnx::Condition_Comparison;
	const Cmp C = Cond.comparison();

	switch (C)
	{
		case metaonnx::Condition_Comparison_CMP_EQ:
			return Left == Right;
		case metaonnx::Condition_Comparison_CMP_NE:
			return Left != Right;
		case metaonnx::Condition_Comparison_CMP_LT:
			return Left < Right;
		case metaonnx::Condition_Comparison_CMP_LE:
			return Left <= Right;
		case metaonnx::Condition_Comparison_CMP_GT:
			return Left > Right;
		case metaonnx::Condition_Comparison_CMP_GE:
			return Left >= Right;
		default:
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Condition: unknown comparison %d"), (int32)C);
			return false;
	}
}

// Executes a single ONNX model node: runs pre-actions, resolves symbolic dimensions,
// binds input tensors from the pool, invokes inference, and runs post-actions.
bool FMetaGraphRunner::RunNode(
    const metaonnx::Node& Node,
    const TMap<FString, metaonnx::Node*>& Nodes,
    const TMap<FString, metaonnx::Loop*>& Loops,
    const TMap<FString, metaonnx::Condition*>& Conds,
    FHostMap& Host
)
{

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("\n=== Running node %s ==="), *ToFString(Node.id()));

	for (int i = 0; i < Node.pre_actions_size(); ++i)
	{
		EvalHostAction(Node.pre_actions(i), Host);
	}

	TMap<FString, FSymExprParser::VarDictEntry> SymDict;

	for (int i = 0; i < Node.inputs_size(); ++i)
	{
		const auto& Ib = Node.inputs(i);
		const FString InputName = ToFString(Ib.input_name());
		const FString TensorName = ToFString(Ib.tensor_name());

		for (int j = 0; j < Ib.dims_size(); ++j)
		{
			const metaonnx::DimEntry& dim = Ib.dims(j);
			if (dim.type() == metaonnx::DimEntry_DimType_DIM_SYMBOLIC || dim.type() == metaonnx::DimEntry_DimType_DIM_RUNTIME)
			{
				FSymExprParser::VarDictEntry Entry;
				Entry.TargetTensor = TensorName;
				Entry.Dim = j;
				SymDict.Add(ToFString(dim.expression()), Entry);
			}
		}

		if (InputName == TensorName)
		{
			continue;
		}

		const ModelIOData* SrcTensor = nullptr;
		if (!TensorPool.TryGetTensor(TensorName, SrcTensor) || !SrcTensor)
		{
			LINGO_LOG(
			    EVerbosityLevel::Error, TEXT("Node %s: missing tensor %s for binding to input %s"), *ToFString(Node.id()), *TensorName, *InputName
			);
			return false;
		}

		TensorPool.SetTensor(InputName, ModelIOData(*SrcTensor));
	}

	TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> Workload = ResolveWorkloadForNode(Node);
	if (!Workload)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("MetaGraphRunner: failed to resolve workload for node %s (model_path=%s)"),
		    *ToFString(Node.id()),
		    *ToFString(Node.model_path())
		);
		return false;
	}

	TArray<UE::NNE::FTensorShape> OutputShapes;
	for (int i = 0; i < Node.outputs_size(); ++i)
	{
		const auto& Ob = Node.outputs(i);
		TArray<uint32> CurrentDims;

		for (int j = 0; j < Ob.dims_size(); ++j)
		{
			const metaonnx::DimEntry& dim = Ob.dims(j);
			if (dim.type() == metaonnx::DimEntry_DimType_DIM_SYMBOLIC || dim.type() == metaonnx::DimEntry_DimType_DIM_RUNTIME)
			{
				FString expression = ToFString(dim.expression());
				uint32 Result;
				if (!FSymExprParser::Evaluate(expression, SymDict, Host, TensorPool, Result))
				{
					LINGO_LOG(EVerbosityLevel::Debug, TEXT("Failed to parse symbolic dimension: %s"), *expression);
				}
				CurrentDims.Add(Result);
			}
			else
			{
				CurrentDims.Add(dim.size());
			}
		}

		OutputShapes.Add(UE::NNE::FTensorShape::Make(CurrentDims));
	}

	bool bInferenceSuccess = Workload->Infer(TensorPool, OutputShapes, Config, *ToFString(Node.id()));
	if (!bInferenceSuccess)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Node '%s' failed to run. Aborting."), *ToFString(Node.id()));
		return false;
	}

	for (int i = 0; i < Node.post_actions_size(); ++i)
	{
		EvalHostAction(Node.post_actions(i), Host);
	}

	return true;
}

bool FMetaGraphRunner::RunLoop(
    const metaonnx::Loop& Loop,
    const TMap<FString, metaonnx::Node*>& Nodes,
    const TMap<FString, metaonnx::Loop*>& Loops,
    const TMap<FString, metaonnx::Condition*>& Conds,
    FHostMap& Host
)
{

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Loop: %s"), *ToFString(Loop.id()));

	const FString IterVarName = ToFString(Loop.iteration_var_name());
	FHostValue IterValue;
	IterValue.Set<int64>(0);
	Host.Add(IterVarName, IterValue);

	metaonnx::Condition* Cond = nullptr;
	if (metaonnx::Condition* const* Found = Conds.Find(ToFString(Loop.condition_id())))
	{
		Cond = *Found;
	}
	if (!Cond)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Loop %s: missing condition %s"), *ToFString(Loop.id()), *ToFString(Loop.condition_id()));
		return false;
	}

	while (true)
	{
		FHostValue* IterVal = Host.Find(IterVarName);
		check(IterVal && IterVal->IsType<int64>());

		int64 Iter = IterVal->Get<int64>();
		if (Iter >= static_cast<int64>(Loop.max_iterations()))
		{
			break;
		}

		if (!RunCondition(*Cond, Host))
		{
			break;
		}

		for (int i = 0; i < Loop.subgraph_size(); ++i)
		{
			if (!ExecuteGraphItem(Loop.subgraph(i), Nodes, Loops, Conds, Host))
			{
				return false;
			}
		}

		Host[IterVarName].Set<int64>(Iter + 1);
	}

	return true;
}

// Dispatches a single graph item by type. Checks for cancellation before each item.
bool FMetaGraphRunner::ExecuteGraphItem(
    const metaonnx::GraphItem& Item,
    const TMap<FString, metaonnx::Node*>& Nodes,
    const TMap<FString, metaonnx::Loop*>& Loops,
    const TMap<FString, metaonnx::Condition*>& Conds,
    FHostMap& Host
)
{
	// Check cancel condition — just bail out, Run() will post the appropriate packet
	if (ExternStopSignal())
	{
		return false;
	}

	if (Item.has_node_id())
	{
		const FString NodeId = ToFString(Item.node_id());
		metaonnx::Node* const* NodePtr = Nodes.Find(NodeId);
		if (!NodePtr)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("GraphItem: unknown node_id %s"), *NodeId);
			return false;
		}
		return RunNode(**NodePtr, Nodes, Loops, Conds, Host);
	}

	if (Item.has_host_action())
	{
		EvalHostAction(Item.host_action(), Host);
		return true;
	}

	if (Item.has_loop_id())
	{
		const FString LoopId = ToFString(Item.loop_id());
		metaonnx::Loop* const* LoopPtr = Loops.Find(LoopId);
		if (!LoopPtr)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("GraphItem: unknown loop_id %s"), *LoopId);
			return false;
		}
		return RunLoop(**LoopPtr, Nodes, Loops, Conds, Host);
	}

	if (Item.has_conditional())
	{
		const auto& Branch = Item.conditional();
		const FString CondId = ToFString(Branch.condition_id());

		metaonnx::Condition* const* CondPtr = Conds.Find(CondId);
		if (!CondPtr)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Conditional: unknown condition_id %s"), *CondId);
			return false;
		}

		if (RunCondition(**CondPtr, Host))
		{
			for (int i = 0; i < Branch.then_flow_size(); ++i)
			{
				if (!ExecuteGraphItem(Branch.then_flow(i), Nodes, Loops, Conds, Host))
				{
					return false;
				}
			}
		}
		else
		{
			for (int i = 0; i < Branch.else_flow_size(); ++i)
			{
				if (!ExecuteGraphItem(Branch.else_flow(i), Nodes, Loops, Conds, Host))
				{
					return false;
				}
			}
		}
		return true;
	}

	return true;
}

// Main entry point: builds lookup maps for nodes/loops/conditions, then executes the graph items in order.
// Each graph item can be a model node, a host action, a loop, or a conditional branch.
bool FMetaGraphRunner::Run(const metaonnx::MetaGraph& Graph, TArray<float>& OutSamples)
{

	FHostMap Host;

	TMap<FString, metaonnx::Node*> Nodes;
	TMap<FString, metaonnx::Loop*> Loops;
	TMap<FString, metaonnx::Condition*> Conds;

	for (int i = 0; i < Graph.nodes_size(); ++i)
	{
		metaonnx::Node* N = const_cast<metaonnx::Node*>(&Graph.nodes(i));
		Nodes.Add(ToFString(N->id()), N);
	}
	for (int i = 0; i < Graph.loops_size(); ++i)
	{
		metaonnx::Loop* L = const_cast<metaonnx::Loop*>(&Graph.loops(i));
		Loops.Add(ToFString(L->id()), L);
	}
	for (int i = 0; i < Graph.conditions_size(); ++i)
	{
		metaonnx::Condition* C = const_cast<metaonnx::Condition*>(&Graph.conditions(i));
		Conds.Add(ToFString(C->id()), C);
	}

	for (int i = 0; i < Graph.graph_size(); ++i)
	{
		if (!ExecuteGraphItem(Graph.graph(i), Nodes, Loops, Conds, Host))
		{
			return false;
		}
	}

	// TODO: Remove when callback types are implemented
	// TArray<float> Dummy;
	// Callback(Dummy, true);

	return true;
}
