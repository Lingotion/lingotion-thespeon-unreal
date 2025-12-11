// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "../Core/ModelInput.h"
#include "../Core/Language.h"
#include "../Core/LingotionLogger.h"
#include "InferenceConfig.generated.h"


USTRUCT(BlueprintType)
struct FInferenceConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Config")
    EBackendType BackendType = EBackendType::CPU;

    // Unclear if these scheduling parameters can be used in unreal. 
    // Investigate how threaded inference interacts with game thread.
    // Maybe we need these for some breaking between each model run?
    UPROPERTY(EditAnywhere, Category="Scheduling", meta = (ClampMin = "0.0"))
    double TargetBudgetTime = 0.005;

    UPROPERTY(EditAnywhere, Category="Scheduling", meta = (ClampMin = "0.0"))
    double TargetFrameTime = 0.0167;

    UPROPERTY(EditAnywhere, Category="Scheduling", meta = (ClampMin = "0.0"))
    float BufferSeconds = 0.5f;

    // Module configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Config")
    EThespeonModuleType ModuleType = EThespeonModuleType::L;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Config")
    EEmotion FallbackEmotion = EEmotion::Interest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Config", meta=(ShowOnlyInnerProperties))
    FLingotionLanguage FallbackLanguage;

    FInferenceConfig()
        : FallbackLanguage(TEXT("eng"))
    {}
};

