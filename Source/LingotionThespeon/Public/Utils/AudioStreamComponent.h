// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
// #include "Sound/SoundGenerator.h" // for ISoundGenerator, ISoundGeneratorPtr, FSoundGeneratorInitParams
#include "AudioStreamComponent.generated.h"

class FAudioStreamSharedData;

/** Fires on the game thread when the audio ring buffer transitions from non-empty to empty
 *  after audio had been submitted. Use this to detect "playback finished" without polling.
 *
 *  NOTE: This currently uses buffer-empty as the completion signal. If audio generation
 *  becomes slower than playback (not the case in the current Thespeon pipeline), transient
 *  drains between packets could fire this prematurely. A precise per-session-complete
 *  signal will arrive with the planned AudioSampleRequest unification. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackBufferDrained);

/**
 * Streams synthesized audio to the Unreal audio engine in real time.
 *
 * Receives PCM float data from the inference thread via SubmitAudio() and feeds it
 * to a custom ISoundGenerator for playback. Uses a thread-safe shared ring buffer
 * to bridge the game thread and the audio render thread.
 */
UCLASS(ClassGroup = (Custom), Config = Engine, meta = (BlueprintSpawnableComponent))
class LINGOTIONTHESPEON_API UAudioStreamComponent : public USynthComponent
{
	GENERATED_BODY()

  public:
	UAudioStreamComponent(const FObjectInitializer& ObjectInitializer);

	/** Number of channels in the submitted audio data. */
	int32 InputNumChannels = 1;
	/**
	 * Gain multiplier applied to the output audio signal (1.0 = unity, >1.0 = louder, <1.0 = quieter).
	 * Default 2.0 compensates for the typically low peak amplitude of synthesized speech.
	 * Override per-project in DefaultEngine.ini under [/Script/LingotionThespeon.AudioStreamComponent].
	 */
	UPROPERTY(
	    EditAnywhere, BlueprintReadWrite, Config, Category = "Lingotion Thespeon|Audio", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "8.0")
	)
	float OutputGain = 2.0f;

	/** Broadcast on the game thread when the audio buffer drains after having held audio. */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnPlaybackBufferDrained OnPlaybackBufferDrained;

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

	/**
	 * Returns true when the audio ring buffer has no unconsumed samples remaining.
	 *
	 * Intended use: after UThespeonComponent::OnSynthesisComplete fires, poll this to
	 * detect when the audio stream has finished playing everything the synth produced.
	 * A tiny DSP output latency may still be in flight when this first reports true.
	 *
	 * Thread-safe: callable from the game thread while the audio render thread reads.
	 */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Audio")
	bool IsBufferEmpty() const;

	/**
	 * Returns the number of audio sample frames that have been consumed by the audio render thread
	 * since the stream started (or since the last ResetBuffer() call).
	 *
	 * @param CompensationSamples Optional number of sample frames to subtract, to compensate for
	 * output/device latency between a sample being consumed here and it becoming audible.
	 *
	 * Thread-safe: callable from the game thread while the audio render thread writes.
	 */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Audio")
	int64 GetPlaybackSampleIndex(int32 CompensationSamples = 0) const;

	/** Clears the audio ring buffer and resets playback state. */
	void ResetBuffer();

  protected:
	/**
	 * Polls the shared buffer's drain flag on the game thread and broadcasts
	 * OnPlaybackBufferDrained when the audio render thread reports a drain.
	 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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
