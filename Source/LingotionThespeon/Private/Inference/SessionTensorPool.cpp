// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "SessionTensorPool.h"

Thespeon::Inference::SessionTensorPool::SessionTensorPool() {}
// Fetches a pointer to the ModelIOData if it exists.
bool Thespeon::Inference::SessionTensorPool::TryGetTensor(const FString& Name, ModelIOData*& Out)
{
	if (TUniquePtr<ModelIOData>* Found = TensorMap.Find(Name))
	{
		Out = Found->Get();
		return true;
	}
	Out = nullptr;
	return false;
}

bool Thespeon::Inference::SessionTensorPool::TryGetTensor(const FString& Name, const ModelIOData*& Out) const
{
	if (const TUniquePtr<ModelIOData>* Found = TensorMap.Find(Name))
	{
		Out = Found->Get();
		return true;
	}
	Out = nullptr;
	return false;
}

// replaces a ModelIOData in the tensor pool. Note that this makes old pointers invalid,
// so never call SetTensor if you are not sure that you won't need the previous pointer!
void Thespeon::Inference::SessionTensorPool::SetTensor(const FString& Name, TUniquePtr<ModelIOData>&& TensorData)
{
	// transfer ownership fully to the pool
	TensorMap.Emplace(Name, MoveTemp(TensorData));
}

void Thespeon::Inference::SessionTensorPool::SetTensor(const FString& Name, ModelIOData&& TensorData)
{
	// transfer ownership fully to the pool
	TensorMap.Emplace(Name, MakeUnique<ModelIOData>(MoveTemp(TensorData)));
}

bool Thespeon::Inference::SessionTensorPool::TryRenameTensor(const FString& SourceTensorName, const FString& TargetTensorName)
{
	TUniquePtr<ModelIOData>* Found = TensorMap.Find(SourceTensorName);
	if (!Found)
	{
		return false;
	}

	// Move the unique_ptr out of the old slot...
	TUniquePtr<ModelIOData> Tmp = MoveTemp(*Found); // Found becomes null
	TensorMap.Remove(SourceTensorName);             // remove the old slot

	// ...and insert under the new name
	TensorMap.Emplace(TargetTensorName, MoveTemp(Tmp));
	return true;
}
