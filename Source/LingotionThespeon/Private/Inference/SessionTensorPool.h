// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "ModelIOData.h"

namespace Thespeon
{
namespace Inference
{

/**
 * Owns ModelIOData tensors for a single inference session.
 * Hands out pointers when requested; SetTensor transfers ownership into the pool.
 */
class SessionTensorPool
{
  public:
	SessionTensorPool();

	/** @brief Retrieves a const pointer to a tensor by name.
	 *  @param TensorName Name of the tensor to look up.
	 *  @param OutTensor Output const pointer to the tensor data.
	 *  @return True if the tensor was found, false otherwise. */
	bool TryGetTensor(const FString& TensorName, const ModelIOData*& OutTensor) const;

	/** @brief Retrieves a mutable pointer to a tensor by name.
	 *  @param TensorName Name of the tensor to look up.
	 *  @param OutTensor Output pointer to the tensor data.
	 *  @return True if the tensor was found, false otherwise. */
	bool TryGetTensor(const FString& TensorName, ModelIOData*& OutTensor);

	/** @brief Renames a tensor in the pool.
	 *  @param SourceTensorName Current name of the tensor.
	 *  @param TargetTensorName New name for the tensor.
	 *  @return True if the rename succeeded, false if source not found. */
	bool TryRenameTensor(const FString& SourceTensorName, const FString& TargetTensorName);

	/** @brief Stores a tensor by move, transferring ownership to the pool.
	 *  @param TensorName Name to store the tensor under.
	 *  @param TensorData The tensor data to store (moved). */
	void SetTensor(const FString& TensorName, ModelIOData&& TensorData);

	/** @brief Stores a tensor via unique pointer, transferring ownership to the pool.
	 *  @param TensorName Name to store the tensor under.
	 *  @param TensorData The tensor data to store (moved). */
	void SetTensor(const FString& TensorName, TUniquePtr<ModelIOData>&& TensorData);

  private:
	TMap<FString, TUniquePtr<ModelIOData>> TensorMap;
};
} // namespace Inference
} // namespace Thespeon
