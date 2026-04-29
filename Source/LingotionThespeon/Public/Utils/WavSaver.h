// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"

/**
 * Standard WAV file header (44 bytes, packed).
 *
 * Defaults to 32-bit float mono PCM at 44100 Hz (AudioFormat = 3 = IEEE float).
 * ChunkSize, ByteRate, BlockAlign, and Subchunk2Size are computed at write time.
 */
#pragma pack(push, 1)
struct FWavHeader
{
	char ChunkID[4] = {'R', 'I', 'F', 'F'};
	uint32 ChunkSize = 0;
	char Format[4] = {'W', 'A', 'V', 'E'};

	char Subchunk1ID[4] = {'f', 'm', 't', ' '};
	uint32 Subchunk1Size = 16;
	/** Audio format: 3 = IEEE 754 float. */
	uint16 AudioFormat = 3;
	uint16 NumChannels = 1;
	uint32 SampleRate = 44100;
	uint32 ByteRate = 0;
	uint16 BlockAlign = 0;
	uint16 BitsPerSample = 32;

	char Subchunk2ID[4] = {'d', 'a', 't', 'a'};
	uint32 Subchunk2Size = 0;
};
#pragma pack(pop)

/**
 * Utility for saving raw float PCM audio data to a .wav file on disk.
 */
class LINGOTIONTHESPEON_API FWavSaver
{
  public:
	/**
	 * @brief Writes an array of float samples as a 32-bit float WAV file.
	 *
	 * @param Filename Absolute path for the output .wav file.
	 * @param Samples  The audio sample data (32-bit float, mono, 44100 Hz).
	 * @return true if the file was written successfully.
	 */
	static bool SaveWav(const FString& Filename, const TArray<float>& Samples);
};
