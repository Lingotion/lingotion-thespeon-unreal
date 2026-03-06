// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Utils/AudioStreamComponent.h"
#include "Misc/ScopeLock.h"
#include "HAL/UnrealMemory.h"
#include "Sound/SoundGenerator.h" // ensure this is included

// ---------------- Shared Data ----------------

class FAudioStreamSharedData : public TSharedFromThis<FAudioStreamSharedData, ESPMode::ThreadSafe>
{
  public:
	FCriticalSection Mutex;
	TArray<float> Buffer; // interleaved float samples in [-1..1]
	int64 ReadIndex = 0;

	int32 NumChannels = 1;
	float Gain = 1.0f;

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

bool UAudioStreamComponent::Init(int32& SampleRate)
{
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
	// Return SHARED pointer (ISoundGeneratorPtr is TSharedPtr<...>)
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
