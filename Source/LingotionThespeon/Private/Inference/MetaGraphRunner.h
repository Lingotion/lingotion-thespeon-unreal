// MetaGraphRunner_Thespeon.h

#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include "InferenceWorkload.h"
#include "SessionTensorPool.h"
#include "meta_graph.pb.h"
#include "InferenceSession.h"
#include "Core/ThespeonDataPacket.h"
#include "SessionWorkloadCache.h"
namespace Thespeon
{
namespace Character
{
class CharacterModule;
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
	FMetaGraphRunner(
	    SessionTensorPool& InTensorPool,
	    Thespeon::Character::CharacterModule* InCharacterModule,
	    Thespeon::Inference::FSessionWorkloadCache* InWorkloadCache,
	    const FInferenceConfig& InConfig,
	    Thespeon::Inference::FPostPacketFn InCallback,
	    Thespeon::Inference::FShouldStopFn InStopSignal
	);

	/**
	 * @brief Runs a pre-loaded protobuf meta graph to completion.
	 * @param Graph The MetaGraph protobuf to execute.
	 * @param OutSamples Output array populated with synthesized audio samples.
	 * @return False on any error.
	 */
	bool Run(const metaonnx::MetaGraph& Graph, TArray<float>& OutSamples);

	/** @brief Loads a MetaGraph protobuf definition from disk.
	 *  @param FilePath Path to the protobuf file.
	 *  @param OutGraph Output MetaGraph populated from the file.
	 *  @return True if loading succeeded. */
	static bool LoadMetaGraphFromDisk(const FString& FilePath, metaonnx::MetaGraph& OutGraph);

  private:
	// Host variables can store:
	//  - int64 / float / bool     : scalar math + conditions
	//  - ModelIOData              : tensor-to-host (copy tensor into host) and tensor refs as values
	//  - TArray<float>            : audio buffers / callback aggregation
	//  - FString                  : optional (ScalarLiteral.s); not used in arithmetic
	using FHostValue = TVariant<int64, float, bool, ModelIOData, TArray<float>, TArray<int64>, FString>;
	using FHostMap = TMap<FString, FHostValue>;

	// HostAction / Condition / Node / Loop / GraphItem
	void EvalHostAction(const metaonnx::HostAction& Action, FHostMap& Host);
	bool RunCondition(const metaonnx::Condition& Cond, FHostMap& Host) const;

	bool RunNode(
	    const metaonnx::Node& Node,
	    const TMap<FString, metaonnx::Node*>& Nodes,
	    const TMap<FString, metaonnx::Loop*>& Loops,
	    const TMap<FString, metaonnx::Condition*>& Conds,
	    FHostMap& Host
	);

	bool RunLoop(
	    const metaonnx::Loop& Loop,
	    const TMap<FString, metaonnx::Node*>& Nodes,
	    const TMap<FString, metaonnx::Loop*>& Loops,
	    const TMap<FString, metaonnx::Condition*>& Conds,
	    FHostMap& Host
	);

	bool ExecuteGraphItem(
	    const metaonnx::GraphItem& Item,
	    const TMap<FString, metaonnx::Node*>& Nodes,
	    const TMap<FString, metaonnx::Loop*>& Loops,
	    const TMap<FString, metaonnx::Condition*>& Conds,
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

  private:
	SessionTensorPool& TensorPool;
	FSessionWorkloadCache* WorkloadCache = nullptr;
	Thespeon::Character::CharacterModule* CharacterModule = nullptr;
	const FInferenceConfig& Config;
	FPostPacketFn PostPacketCallback;
	FShouldStopFn ExternStopSignal;
};
} // namespace Thespeon::Inference
