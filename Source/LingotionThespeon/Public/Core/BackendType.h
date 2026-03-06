// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "BackendType.generated.h"

/**
 * Selects which NNE (Neural Network Engine) backend to use for inference.
 */
UENUM(BlueprintType)
enum class EBackendType : uint8
{
	/** Run inference on the CPU. */
	CPU UMETA(DisplayName = "CPU"),
	/** Run inference on the GPU. */
	GPU UMETA(DisplayName = "GPU"),
	/** Use the default backend specified in Lingotion Thespeon runtime settings. */
	None UMETA(DisplayName = "Default")
};