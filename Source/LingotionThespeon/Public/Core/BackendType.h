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
	/** Run inference on the GPU. Windows only; other platforms fall back to CPU with a warning. */
	GPU UMETA(DisplayName = "GPU"),
	/** Use the default backend specified in the Lingotion Thespeon runtime settings. Shown as "Default" in the editor. */
	None UMETA(DisplayName = "Default")
};