// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "ModelIOData.h"

namespace Thespeon
{
	namespace Inference
	{

		/// @brief  Owns ModelIOData tensors. Hands out pointers when requested, SetTensor transfers ownership.
		class SessionTensorPool
		{
		public:
			SessionTensorPool();
			// for static context
    		bool TryGetTensor(const FString& TensorName, const ModelIOData*& OutTensor) const;
    		bool TryGetTensor(const FString& TensorName, ModelIOData*& OutTensor);
			bool TryRenameTensor(const FString& SourceTensorName, const FString& TargetTensorName);
			void SetTensor(const FString& TensorName, ModelIOData&& TensorData); 
		private:
			// unique ptr so that only a single owner can exist
			TMap<FString, TUniquePtr<ModelIOData>> TensorMap;
		};
	}
}
