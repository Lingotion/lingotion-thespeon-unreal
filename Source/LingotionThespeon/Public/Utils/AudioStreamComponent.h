// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
// #include "Sound/SoundGenerator.h" // for ISoundGenerator, ISoundGeneratorPtr, FSoundGeneratorInitParams
#include "AudioStreamComponent.generated.h"

class FAudioStreamSharedData;

/**
 * Streams synthesized audio to the Unreal audio engine in real time.
 *
 * Receives PCM float data from the inference thread via SubmitAudio() and feeds it
 * to a custom ISoundGenerator for playback. Uses a thread-safe shared ring buffer
 * to bridge the game thread and the audio render thread.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LINGOTIONTHESPEON_API UAudioStreamComponent : public USynthComponent
{
	GENERATED_BODY()

  public:
	/** Number of channels in the submitted audio data. */
	int32 InputNumChannels = 1;
	/** Gain multiplier applied to the output audio signal. */
	float OutputGain = 1.0f;

	/**
	 * Submits raw PCM float audio data for playback.
	 *
	 * @param Data Pointer to the float sample buffer.
	 * @param NumSamples Number of float samples in the buffer.
	 */
	void SubmitAudio(const float* Data, int32 NumSamples);

	/**
	 * Submits interleaved PCM float audio data for streaming playback.
	 *
	 * @param AudioData Array of interleaved float samples.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Audio")
	void SubmitAudioToStream(const TArray<float>& AudioData);

	/** Clears the audio ring buffer and resets playback state. */
	void ResetBuffer();

  protected:
	/**
	 * Initializes the synth component and retrieves the device sample rate.
	 *
	 * @param SampleRate Receives the audio device sample rate.
	 * @return true if initialization succeeded.
	 */
	bool Init(int32& SampleRate) override;

	/**
	 * Creates the custom sound generator that reads from the shared ring buffer.
	 *
	 * @param InParams Initialization parameters from the audio engine.
	 * @return Shared pointer to the created sound generator.
	 */
	ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

  private:
	TSharedPtr<FAudioStreamSharedData, ESPMode::ThreadSafe> Shared;
	int32 DeviceSampleRate = 0;
};
