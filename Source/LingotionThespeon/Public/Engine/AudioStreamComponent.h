// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
//#include "Sound/SoundGenerator.h" // for ISoundGenerator, ISoundGeneratorPtr, FSoundGeneratorInitParams
#include "AudioStreamComponent.generated.h"



// Forward-declare an external shared-data type (not nested/private)
class FAudioStreamSharedData;

UCLASS()
class LINGOTIONTHESPEON_API UAudioStreamComponent : public USynthComponent
{
    GENERATED_BODY()

public:
    int32  InputNumChannels = 1;
    float  OutputGain       = 1.0f;

    void SubmitAudio(const float* Data, int32 NumSamples);
    void SubmitAudio(const TArray<float>& Interleaved);
    void ResetBuffer();

protected:
    virtual bool Init(int32& SampleRate) override;

    // UE 5.6 signature: return a SHARED pointer
    virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
    TSharedPtr<FAudioStreamSharedData, ESPMode::ThreadSafe> Shared;
    int32 DeviceSampleRate = 0;
};
