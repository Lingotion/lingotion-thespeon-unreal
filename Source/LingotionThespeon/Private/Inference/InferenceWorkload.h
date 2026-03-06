// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
 */
class InferenceWorkload
{
  public:
	/** @brief Creates a model instance from the given NNE model data and backend type.
	 *  @param ModelData The NNE model data asset to create an instance from.
	 *  @param BackendType The backend (CPU/GPU) to use for inference. */
	InferenceWorkload(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType);

	/** @brief Binds input tensors from TensorPool, allocates output tensors with the given shapes,
	 *  runs synchronous inference, and writes outputs back to the TensorPool.
	 *  @param TensorPool The session tensor pool providing inputs and receiving outputs.
	 *  @param OutputTensorShapes Expected shapes for each output tensor.
	 *  @param Config Inference configuration settings.
	 *  @param debugName Optional label for debug logging. */
	void Infer(
	    SessionTensorPool& TensorPool,
	    const TConstArrayView<UE::NNE::FTensorShape>& OutputTensorShapes,
	    const FInferenceConfig& Config,
	    FString debugName = ""
	);

	/** @brief Returns the output tensor descriptors for this workload's model.
	 *  @return View of the output tensor descriptors. */
	TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDesc();

  private:
	SIZE_T CalculateByteSize(const UE::NNE::FTensorShape& Shape, uint32 ByteSize); // helper for correct allocation.
	bool ValidateOutputs(const TArray<ModelIOData>& ModelIO); // not strictly necessary just for sanity. Remove when we are ready.
	FString GetDataAsString(const ModelIOData& Data);         // For debugging.
	bool AnyZero(const TArray<ModelIOData>& ModelIO);         // For debugging.
	FString FormatMel(const ModelIOData& MelData);            // For debugging.
	// For now only allow the IModelInstanceCPU subclass of IModelInstanceRunSync. Later we want to change this ptr to IModelInstanceRunSync and maybe
	// have different implementations of InferenceWorkload for each runtime.
	TSharedPtr<UE::NNE::IModelInstanceRunSync> ModelInstance;
	bool SetCPU(const TStrongObjectPtr<UNNEModelData>& ModelData);
	bool SetGPU(const TStrongObjectPtr<UNNEModelData>& ModelData);
	void SetModelinstance(const TStrongObjectPtr<UNNEModelData>& ModelData, EBackendType BackendType);
};
} // namespace Inference
} // namespace Thespeon
