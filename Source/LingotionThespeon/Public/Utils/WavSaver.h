// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"

/**
 * Standard WAV file header (44 bytes, packed).
 *
 * Defaults to 32-bit IEEE 754 float, mono, 44100 Hz (AudioFormat = 3).
 * ChunkSize, ByteRate, BlockAlign, and Subchunk2Size are computed at write time.
 */
#pragma pack(push, 1)
struct FWavHeader
{
	/** RIFF chunk identifier, always "RIFF". */
	char ChunkID[4] = {'R', 'I', 'F', 'F'};
	/** Size of the rest of the file after this field: 36 + Subchunk2Size. Computed at write time. */
	uint32 ChunkSize = 0;
	/** RIFF form type, always "WAVE". */
	char Format[4] = {'W', 'A', 'V', 'E'};

	/** Format chunk identifier, always "fmt " (with a trailing space). */
	char Subchunk1ID[4] = {'f', 'm', 't', ' '};
	/** Size of the format chunk that follows: 16 bytes. */
	uint32 Subchunk1Size = 16;
	/** Audio format: 3 = IEEE 754 float. */
	uint16 AudioFormat = 3;
	/** Number of interleaved channels: 1 (mono). */
	uint16 NumChannels = 1;
	/** Sample frames per second: 44100. */
	uint32 SampleRate = 44100;
	/** Bytes per second: SampleRate * BlockAlign. Computed at write time. */
	uint32 ByteRate = 0;
	/** Bytes per sample frame across all channels: NumChannels * 4. Computed at write time. */
	uint16 BlockAlign = 0;
	/** Bits per sample: 32. */
	uint16 BitsPerSample = 32;

	/** Data chunk identifier, always "data". */
	char Subchunk2ID[4] = {'d', 'a', 't', 'a'};
	/** Size of the sample data in bytes. Computed at write time. */
	uint32 Subchunk2Size = 0;
};
#pragma pack(pop)

/**
 * Utility for saving raw 32-bit float audio data to a .wav file on disk.
 */
class LINGOTIONTHESPEON_API FWavSaver
{
  public:
	/**
	 * @brief Writes an array of float samples as a 32-bit float WAV file.
	 *
	 * @param Filename Output path. An existing file is overwritten.
	 * @param Samples  The audio sample data (32-bit float, mono, 44100 Hz).
	 * @return true if the file was written; false if Samples is empty (with an error logged) or the file write fails.
	 */
	static bool SaveWav(const FString& Filename, const TArray<float>& Samples);
};
