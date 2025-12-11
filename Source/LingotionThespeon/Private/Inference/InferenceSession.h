// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "Containers/Queue.h"
#include "SessionTensorPool.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "HAL/Runnable.h"
#include "Core/ThespeonDataPacket.h"


namespace Thespeon
{

	namespace Inference
    {
		// abstract class whose implementation handles a complete inference chain from input to output. 
		// Runs several workloads according to the particular implementation's inference scheme.
		class InferenceSession : public FRunnable
		{
		public:
		    using TOnDataSynthesized = TDelegate<void(const Thespeon::Core::FAnyPacket&, float)>;
		
		    TOnDataSynthesized OnDataSynthesized;

		    InferenceSession(const FLingotionModelInput& InInput, const FInferenceConfig InConfig, const FString& InSessionID)
		        : InputConfig(InConfig), SessionID(InSessionID)
		    {
				// Copy all input data from struct
				Input = InInput;
			}
		
		    virtual ~InferenceSession()
		    {
		        Stop();
		    }

		    virtual bool Init() override 
			{ 
				return true;
			}

		    virtual uint32 Run() override = 0;
		    virtual void Stop() override {}
		    virtual void Exit() override {}

		
		protected:
			FLingotionModelInput Input;
		    FInferenceConfig InputConfig;
		    FString SessionID;
		    SessionTensorPool TensorPool;
		};

	}
}
