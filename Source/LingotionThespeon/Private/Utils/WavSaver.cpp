// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Utils/WavSaver.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Core/LingotionLogger.h"

bool FWavSaver::SaveWav(const FString& Filename, const TArray<float>& Samples)
{
	if (Samples.Num() == 0)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("No float samples to write."));
		return false;
	}

	const int32 NumFloatBytes = Samples.Num() * sizeof(float);

	FWavHeader H;
	H.BlockAlign = H.NumChannels * sizeof(float);
	H.ByteRate = H.SampleRate * H.BlockAlign;
	H.Subchunk2Size = NumFloatBytes;
	H.ChunkSize = 36 + H.Subchunk2Size;

	FBufferArchive Ar;
	Ar.Serialize(&H, sizeof(FWavHeader));
	Ar.Serialize((void*)Samples.GetData(), NumFloatBytes);

	bool bOK = FFileHelper::SaveArrayToFile(Ar, *Filename);

	Ar.FlushCache();
	Ar.Empty();

	return bOK;
}
