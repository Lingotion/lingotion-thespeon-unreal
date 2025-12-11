// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ModelInput.h"
#include "LingotionBlueprintLibrary.generated.h"

UCLASS()
class ULingotionBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon|JSON")
    static bool ParseModelInputFromJson(const FString& JsonString, FLingotionModelInput& OutModelInput);
    /**
        * Saves returned Lingotion Synthesized data as a .wav file at the target location.
        * 
        * @param Filename Path to save file name
        * @param Samples Array of samples
        * @return true if succeeded, false if not.
        */
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon|Audio")
    static bool SaveAudioAsWav(const FString& Filename,
                                         const TArray<float>& Samples);
};
