// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "Engine/InferenceConfig.h"
#include "SessionTensorPool.h"
#include "Core/BackendType.h"

namespace Thespeon
{
namespace Inference
{
/**
 * Wraps a single NNE model instance and runs inference on it.
 *
 * Each InferenceWorkload owns one IModelInstanceRunSync (CPU or GPU) and is responsible for
 * binding input tensors from the SessionTensorPool, running inference, and writing outputs back.
 *
 * The source IModel (CPU or GPU) is retained after construction so the workload pool can create
 * additional model instances on any thread without requiring game-thread UObject operations.
 */
class InferenceWorkload
{
  public:
	/** @brief Creates a model instance from the given NNE model data and backend type.
	 *  @param ModelData The NNE model data asset to create an instance from.
	 *  @param BackendType The backend (CPU/GPU) to use for inference. */
	InferenceWorkload(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType);

	/** @brief Creates a new InferenceWorkload by creating a fresh model instance from an existing IModel.
	 *  Does NOT require the game thread (no StaticLoadObject or UObject access).
	 *  @param InModelCPU The source CPU model to create an instance from (may be null if GPU).
	 *  @param InModelGPU The source GPU model to create an instance from (may be null if CPU).
	 *  @param InBackend The backend type for this workload.
	 *  @return A new workload with its own exclusive model instance, or nullptr on failure. */
	static TSharedPtr<InferenceWorkload, ESPMode::ThreadSafe>
	CreateFromModel(TSharedPtr<UE::NNE::IModelCPU> InModelCPU, TSharedPtr<UE::NNE::IModelGPU> InModelGPU, EBackendType InBackend);

	/** @brief Binds input tensors from TensorPool, allocates output tensors with the given shapes,
	 *  runs synchronous inference, and writes outputs back to the TensorPool.
	 *  @param TensorPool The session tensor pool providing inputs and receiving outputs.
	 *  @param OutputTensorShapes Expected shapes for each output tensor.
	 *  @param Config Inference configuration settings.
	 *  @param debugName Optional label for debug logging.
	 * 	@return true if model run succeeded, else false
	 * */
	bool Infer(
	    SessionTensorPool& TensorPool,
	    const TConstArrayView<UE::NNE::FTensorShape>& OutputTensorShapes,
	    const FInferenceConfig& Config,
	    FString debugName = ""
	);

	/** @brief Returns the output tensor descriptors for this workload's model.
	 *  @return View of the output tensor descriptors. */
	TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDesc();

	/** @brief Returns the stored source CPU model (used by the workload pool to create new instances). */
	TSharedPtr<UE::NNE::IModelCPU> GetSourceModelCPU() const
	{
		return SourceModelCPU;
	}

	/** @brief Returns the stored source GPU model (used by the workload pool to create new instances). */
	TSharedPtr<UE::NNE::IModelGPU> GetSourceModelGPU() const
	{
		return SourceModelGPU;
	}

	/** @brief Returns the backend type this workload was created for. */
	EBackendType GetBackendType() const
	{
		return Backend;
	}

  private:
	/** Private default constructor for use by CreateFromModel factory. */
	InferenceWorkload() = default;

	SIZE_T CalculateByteSize(const UE::NNE::FTensorShape& Shape, uint32 ByteSize); // helper for correct allocation.
	bool ValidateOutputs(const TArray<ModelIOData>& ModelIO); // not strictly necessary just for sanity. Remove when we are ready.
	FString GetDataAsString(const ModelIOData& Data);         // For debugging.
	bool AnyZero(const TArray<ModelIOData>& ModelIO);         // For debugging.
	FString FormatMel(const ModelIOData& MelData);            // For debugging.

	TSharedPtr<UE::NNE::IModelInstanceRunSync> ModelInstance;

	/** Source models retained for creating additional instances in the workload pool. */
	TSharedPtr<UE::NNE::IModelCPU> SourceModelCPU;
	TSharedPtr<UE::NNE::IModelGPU> SourceModelGPU;
	EBackendType Backend = EBackendType::None;

	bool SetCPU(const TStrongObjectPtr<UNNEModelData>& ModelData);
	bool SetGPU(const TStrongObjectPtr<UNNEModelData>& ModelData);
	void SetModelinstance(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType);
};
} // namespace Inference
} // namespace Thespeon
