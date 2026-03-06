// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
	TensorMap.Emplace(Name, MakeUnique<ModelIOData>(TensorData));
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
