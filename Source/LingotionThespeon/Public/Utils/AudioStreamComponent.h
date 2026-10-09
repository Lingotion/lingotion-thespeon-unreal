// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "AudioStreamComponent.generated.h"

class FAudioStreamSharedData;

/** Fires on the game thread when the shared audio buffer transitions from non-empty to empty
 *  after audio had been submitted. Use this to detect "playback finished" without polling.
 *  Fires at most once per tick, only while the component ticks, and never from ResetBuffer().
 *
 *  NOTE: This uses buffer-empty as the completion signal. If audio generation is slower than
 *  playback, transient drains between packets fire it early. To detect the end of a session,
 *  only treat a drain after UThespeonComponent::OnSynthesisComplete as final. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackBufferDrained);

/**
 * Streams synthesized audio to the Unreal audio engine in real time.
 *
 * Receives 32-bit float audio on the game thread via SubmitAudioToStream() (or SubmitAudio() from C++),
 * e.g. from UThespeonComponent::OnAudioReceived, and feeds it to a custom ISoundGenerator for playback.
 * A lock-protected shared audio buffer bridges the game thread and the audio render thread.
 * The stream always runs at 44100 Hz.
 */
UCLASS(ClassGroup = (Custom), Config = Engine, meta = (BlueprintSpawnableComponent))
class LINGOTIONTHESPEON_API UAudioStreamComponent : public USynthComponent
{
	GENERATED_BODY()

  public:
	UAudioStreamComponent(const FObjectInitializer& ObjectInitializer);

	/** Number of interleaved channels in the submitted audio data. Read when the component initializes, so set it before then. Thespeon audio is
	 * mono. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon|Audio", meta = (ClampMin = "1"))
	int32 InputNumChannels = 1;
	/**
	 * Gain multiplier applied to the output audio signal (1.0 = unity, >1.0 = louder, <1.0 = quieter).
	 * Default 2.0 compensates for the typically low peak amplitude of synthesized speech.
	 * Override per-project in DefaultEngine.ini under [/Script/LingotionThespeon.AudioStreamComponent].
	 * A changed value takes effect on the next SubmitAudio call.
	 */
	UPROPERTY(
	    EditAnywhere, BlueprintReadWrite, Config, Category = "Lingotion Thespeon|Audio", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "8.0")
	)
	float OutputGain = 2.0f;

	/** Broadcast on the game thread when the audio buffer drains after having held audio. */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnPlaybackBufferDrained OnPlaybackBufferDrained;

	/**
	 * Submits raw 32-bit float audio data (44100 Hz, interleaved per InputNumChannels) for playback.
	 * Call from the game thread. A null pointer or NumSamples <= 0 is ignored. C++ only; the Blueprint
	 * equivalent is SubmitAudioToStream.
	 *
	 * @param Data Pointer to the float sample buffer.
	 * @param NumSamples Number of float samples in the buffer.
	 */
	void SubmitAudio(const float* Data, int32 NumSamples);

	/**
	 * Submits interleaved 32-bit float audio data (44100 Hz) for streaming playback. Call from the game thread.
	 * C++ callers with a raw buffer can use SubmitAudio(const float* Data, int32 NumSamples) instead.
	 *
	 * @param AudioData Array of interleaved float samples.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Audio")
	void SubmitAudioToStream(const TArray<float>& AudioData);

	/**
	 * Returns true when the shared audio buffer has no unconsumed samples remaining.
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
	 * since the stream started (or since the last ResetBuffer() call). Not reset per synthesis session:
	 * call ResetBuffer() at session start to line up with per-session sample indices.
	 *
	 * @param CompensationSamples Optional number of sample frames to subtract, to compensate for
	 * output/device latency between a sample being consumed here and it becoming audible.
	 *
	 * Thread-safe: callable from the game thread while the audio render thread writes.
	 */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Audio")
	int64 GetPlaybackSampleIndex(int32 CompensationSamples = 0) const;

	/** Clears the shared audio buffer and resets the playback position. Does not broadcast OnPlaybackBufferDrained. */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Audio")
	void ResetBuffer();

  protected:
	/**
	 * Polls the shared buffer's drain flag on the game thread and broadcasts
	 * OnPlaybackBufferDrained when the audio render thread reports a drain.
	 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Initializes the synth component: reports a fixed 44100 Hz sample rate and clears the shared buffer.
	 *
	 * @param SampleRate Receives the stream's sample rate (always 44100).
	 * @return true if initialization succeeded.
	 */
	bool Init(int32& SampleRate) override;

	/**
	 * Creates the custom sound generator that reads from the shared audio buffer.
	 *
	 * @param InParams Initialization parameters from the audio engine.
	 * @return Shared pointer to the created sound generator.
	 */
	ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

  private:
	TSharedPtr<FAudioStreamSharedData, ESPMode::ThreadSafe> Shared;
	int32 DeviceSampleRate = 0;
};
