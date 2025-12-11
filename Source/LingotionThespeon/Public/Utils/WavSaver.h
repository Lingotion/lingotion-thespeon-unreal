// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"

#pragma pack(push, 1)
struct FWavHeader
{
    char  ChunkID[4]      = { 'R','I','F','F' };
    uint32 ChunkSize      = 0;
    char  Format[4]       = { 'W','A','V','E' };

    char  Subchunk1ID[4]  = { 'f','m','t',' ' };
    uint32 Subchunk1Size  = 16;
    uint16 AudioFormat    = 3;
    uint16 NumChannels    = 1;
    uint32 SampleRate     = 44100;
    uint32 ByteRate       = 0;
    uint16 BlockAlign     = 0;
    uint16 BitsPerSample  = 32;

    char  Subchunk2ID[4]  = { 'd','a','t','a' };
    uint32 Subchunk2Size  = 0;
};
#pragma pack(pop)
class LINGOTIONTHESPEON_API FWavSaver
{
    public:
        static bool SaveWav(const FString& Filename,
                                             const TArray<float>& Samples);
};

