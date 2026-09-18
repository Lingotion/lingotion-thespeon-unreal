// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Utils/AudioStreamComponent.h"
#include "Misc/ScopeLock.h"
#include "HAL/UnrealMemory.h"
#include "Sound/SoundGenerator.h" // ensure this is included
#include "Core/LingotionLogger.h"
#if !WITH_EDITOR
#include "Engine/Engine.h" // GEngine for the Demo-only on-screen indicator
#endif
#include <atomic>

// ---------------- Shared Data ----------------

class FAudioStreamSharedData : public TSharedFromThis<FAudioStreamSharedData, ESPMode::ThreadSafe>
{
  public:
	mutable FCriticalSection Mutex;
	TArray<float> Buffer; // interleaved float samples in [-1..1]
	int64 ReadIndex = 0;

	int32 NumChannels = 1;
	float Gain = 1.0f;

	// Monotonic count of raw samples consumed by the audio render thread since the last Clear().
	// Unlike ReadIndex, this is never reduced by buffer compaction.
	std::atomic<int64> SamplesConsumed{0};

	// Tracks whether audio has been submitted that has not yet been fully consumed.
	// Set by Submit; cleared by Pop when the buffer transitions to empty.
	std::atomic<bool> bHadAudio{false};
	// Set by Pop when a drain transition is detected; consumed by the game-thread tick.
	// NOTE: This currently fires whenever the buffer empties after audio has been submitted.
	// In a pipeline where audio generation is slower than playback, transient drains between
	// packets could fire this prematurely.
	std::atomic<bool> bDrainPending{false};

	void Submit(const float* Data, int32 NumSamples)
	{
		if (!Data || NumSamples <= 0)
		{
			return;
		}
		FScopeLock Lock(&Mutex);
		const int32 OldNum = Buffer.Num();
		Buffer.AddUninitialized(NumSamples);
		FMemory::Memcpy(Buffer.GetData() + OldNum, Data, NumSamples * sizeof(float));
		bHadAudio.store(true, std::memory_order_relaxed);
		// New audio invalidates any previously-pending drain notification.
		bDrainPending.store(false, std::memory_order_relaxed);
	}

	int32 Pop(float* OutAudio, int32 NumSamples)
	{
		int32 Copied = 0;
		{
			FScopeLock Lock(&Mutex);
			const int64 Available = Buffer.Num() - ReadIndex;
			if (Available > 0)
			{
				Copied = FMath::Min<int32>(NumSamples, static_cast<int32>(Available));
				FMemory::Memcpy(OutAudio, Buffer.GetData() + ReadIndex, Copied * sizeof(float));
				ReadIndex += Copied;
				SamplesConsumed.fetch_add(Copied, std::memory_order_relaxed);

				// Compact occasionally
				const int64 Remaining = Buffer.Num() - ReadIndex;
				if (ReadIndex > 262144 && Remaining < Buffer.Num() / 2)
				{
					if (Remaining > 0)
					{
						FMemory::Memmove(Buffer.GetData(), Buffer.GetData() + ReadIndex, Remaining * sizeof(float));
					}
					Buffer.SetNum(Remaining);
					ReadIndex = 0;
				}

				// Detect transition: just consumed samples and buffer is now empty.
				if ((Buffer.Num() - ReadIndex) <= 0 && bHadAudio.exchange(false, std::memory_order_relaxed))
				{
					bDrainPending.store(true, std::memory_order_relaxed);
				}
			}
		}

		if (Gain != 1.0f && Copied > 0)
		{
			for (int32 i = 0; i < Copied; ++i)
			{
				OutAudio[i] *= Gain;
			}
		}
		return Copied;
	}

	void Clear()
	{
		FScopeLock Lock(&Mutex);
		Buffer.Reset();
		ReadIndex = 0;
		SamplesConsumed.store(0, std::memory_order_relaxed);
		bHadAudio.store(false, std::memory_order_relaxed);
		bDrainPending.store(false, std::memory_order_relaxed);
	}

	bool IsEmpty() const
	{
		FScopeLock Lock(&Mutex);
		return (Buffer.Num() - ReadIndex) <= 0;
	}
};

// ---------------- Generator ----------------

class FAudioStreamGenerator : public ISoundGenerator
{
  public:
	explicit FAudioStreamGenerator(TSharedPtr<FAudioStreamSharedData, ESPMode::ThreadSafe> InShared) : Shared(MoveTemp(InShared)) {}

	void OnBeginGenerate() override {}
	void OnEndGenerate() override {}

	// Mixer asks for NumSamples; we return exactly NumSamples (fill missing with silence)
	int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
	{
		check(OutAudio);
		FMemory::Memzero(OutAudio, NumSamples * sizeof(float));
		if (Shared.IsValid())
		{
			Shared->Pop(OutAudio, NumSamples);
		}
		return NumSamples;
	}

  private:
	TSharedPtr<FAudioStreamSharedData, ESPMode::ThreadSafe> Shared;
};

// ---------------- Component ----------------

UAudioStreamComponent::UAudioStreamComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAudioStreamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Shared.IsValid() && Shared->bDrainPending.exchange(false, std::memory_order_relaxed))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Audio playback complete (buffer drained)"));
		OnPlaybackBufferDrained.Broadcast();
	}
}

bool UAudioStreamComponent::Init(int32& SampleRate)
{
	SampleRate = 44100;
	DeviceSampleRate = SampleRate;
	NumChannels = FMath::Max(1, InputNumChannels);

	if (!Shared.IsValid())
	{
		Shared = MakeShared<FAudioStreamSharedData, ESPMode::ThreadSafe>();
	}
	Shared->NumChannels = NumChannels;
	Shared->Gain = OutputGain;
	Shared->Clear();

	return true;
}

ISoundGeneratorPtr UAudioStreamComponent::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	return MakeShared<FAudioStreamGenerator, ESPMode::ThreadSafe>(Shared);
}

void UAudioStreamComponent::SubmitAudio(const float* Data, int32 NumSamples)
{
	if (!Shared.IsValid())
	{
		Shared = MakeShared<FAudioStreamSharedData, ESPMode::ThreadSafe>();
	}
	Shared->NumChannels = FMath::Max(1, InputNumChannels);
	Shared->Gain = OutputGain;
	Shared->Submit(Data, NumSamples);
}

void UAudioStreamComponent::SubmitAudioToStream(const TArray<float>& AudioData)
{
	SubmitAudio(AudioData.GetData(), AudioData.Num());
}

void UAudioStreamComponent::ResetBuffer()
{
	if (Shared.IsValid())
	{
		Shared->Clear();
	}
}

bool UAudioStreamComponent::IsBufferEmpty() const
{
	return Shared.IsValid() ? Shared->IsEmpty() : true;
}

int64 UAudioStreamComponent::GetPlaybackSampleIndex(int32 CompensationSamples) const
{
	if (!Shared.IsValid())
	{
		return 0;
	}
	const int32 Channels = FMath::Max(1, Shared->NumChannels);
	const int64 ConsumedFrames = Shared->SamplesConsumed.load(std::memory_order_relaxed) / Channels;
	const int64 Audible = ConsumedFrames - CompensationSamples;
	return Audible < 0 ? 0 : Audible;
}
