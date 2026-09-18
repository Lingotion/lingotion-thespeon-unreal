// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

// MetaGraphRunner_Thespeon.h

#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include "InferenceWorkload.h"
#include "SessionTensorPool.h"
#include "Core/meta_graph.pb.h"
#include "InferenceSession.h"
#include "Core/ThespeonDataPacket.h"
#include "SessionWorkloadCache.h"
namespace Thespeon
{
namespace Core
{
class Module;
}
} // namespace Thespeon

namespace Thespeon::Inference
{
// The metagraph runner should have some callback "this packet is ready, send it"
using FPostPacketFn = TUniqueFunction<void(const Thespeon::Core::FThespeonDataPacket& /*DataPacket*/)>;
using FShouldStopFn = TUniqueFunction<bool const()>;

/**
 * Executes a protobuf-defined meta graph that orchestrates inference workloads.
 *
 * Interprets MetaGraph nodes, loops, conditions, and host actions to drive the
 * full TTS inference pipeline. Posts synthesized audio packets via a callback.
 */
class FMetaGraphRunner
{
  public:
	/**
	 * @param InModule Module whose file mappings resolve this graph's node ids to workloads —
	 *        a CharacterModule for the voice graph, a LanguageModule for the phonemizer graph.
	 */
	FMetaGraphRunner(
	    SessionTensorPool& InTensorPool,
	    Thespeon::Core::Module* InModule,
	    Thespeon::Inference::FSessionWorkloadCache* InWorkloadCache,
	    const FInferenceConfig& InConfig,
	    Thespeon::Inference::FPostPacketFn InCallback,
	    Thespeon::Inference::FShouldStopFn InStopSignal
	);

	/**
	 * @brief Runs a pre-loaded protobuf meta graph to completion.
	 *
	 * The graph is treated as strictly read-only, so a single parsed graph (see
	 * Module::GetMetaGraph) may back concurrent sessions.
	 *
	 * @param Graph The MetaGraph protobuf to execute.
	 * @return False on any error.
	 */
	bool Run(const metaonnx::MetaGraph& Graph);

  private:
	// Host variables can store:
	//  - int64 / float / bool     : scalar math + conditions
	//  - ModelIOData              : tensor-to-host (copy tensor into host) and tensor refs as values
	//  - TArray<float>            : audio buffers / callback aggregation
	//  - FString                  : optional (ScalarLiteral.s); not used in arithmetic
	using FHostValue = TVariant<int64, float, bool, ModelIOData, TArray<float>, TArray<int64>, FString>;
	using FHostMap = TMap<FString, FHostValue>;

	/** @brief Validates TensorPool contents against Graph.inputs (presence, dtype, dims,
	 *  and cross-input symbolic dim consistency), then fills in default values for
	 *  missing optional inputs.
	 *  @param Graph The MetaGraph whose declared input contract to validate against.
	 *  @return False if any declared input is missing, has the wrong dtype, or has
	 *  mismatched dimensions. */
	bool ValidateAndFillInputs(const metaonnx::MetaGraph& Graph);

	// HostAction / Condition / Node / Loop / GraphItem
	void EvalHostAction(const metaonnx::HostAction& Action, FHostMap& Host);
	bool RunCondition(const metaonnx::Condition& Cond, FHostMap& Host) const;

	bool RunNode(
	    const metaonnx::Node& Node,
	    const TMap<FString, const metaonnx::Node*>& Nodes,
	    const TMap<FString, const metaonnx::Loop*>& Loops,
	    const TMap<FString, const metaonnx::Condition*>& Conds,
	    FHostMap& Host
	);

	bool RunLoop(
	    const metaonnx::Loop& Loop,
	    const TMap<FString, const metaonnx::Node*>& Nodes,
	    const TMap<FString, const metaonnx::Loop*>& Loops,
	    const TMap<FString, const metaonnx::Condition*>& Conds,
	    FHostMap& Host
	);

	bool ExecuteGraphItem(
	    const metaonnx::GraphItem& Item,
	    const TMap<FString, const metaonnx::Node*>& Nodes,
	    const TMap<FString, const metaonnx::Loop*>& Loops,
	    const TMap<FString, const metaonnx::Condition*>& Conds,
	    FHostMap& Host
	);

	// Helpers
	static float HostToFloat(const FHostValue& V);
	static bool HostToBool(const FHostValue& V);
	static bool ScalarLiteralToHostValue(const metaonnx::ScalarLiteral& Lit, FHostValue& Out);
	static bool ResolveValueRef(const metaonnx::ValueRef& Ref, const SessionTensorPool& TensorPool, const FHostMap& Host, FHostValue& Out);

	TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe> ResolveWorkloadForNode(const metaonnx::Node& Node) const;

	// Tensor creation helpers
	static int64 NumElements(const TArray<int64>& Dims);
	static UE::NNE::FTensorShape MakeShapeFromDims(const TArray<int64>& Dims);

	// Builds a tensor from a TensorCreate spec: resolves static/symbolic/runtime dims,
	// then fills it per the spec's Fill mode. Returns null on failure.
	TUniquePtr<ModelIOData> BuildTensorCreateArray(const metaonnx::TensorCreate& A, const FHostMap& Host);

  private:
	SessionTensorPool& TensorPool;
	FSessionWorkloadCache* WorkloadCache = nullptr;
	// Resolves node ids to workload IDs. Base-class pointer so either a character or a
	// language module can back the graph; only GetInternalModelID is used.
	Thespeon::Core::Module* OwningModule = nullptr;
	const FInferenceConfig& Config;
	FPostPacketFn PostPacketCallback;
	FShouldStopFn ExternStopSignal;
};
} // namespace Thespeon::Inference
