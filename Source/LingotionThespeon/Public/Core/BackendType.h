// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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