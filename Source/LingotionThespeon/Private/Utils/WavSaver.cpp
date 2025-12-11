// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Utils/WavSaver.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"

bool FWavSaver::SaveWav(const FString& Filename,
                                         const TArray<float>& Samples)
{
    if (Samples.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("No float samples to write."));
        return false;
    }

    const int32 NumFloatBytes = Samples.Num() * sizeof(float);

    FWavHeader H;
    H.BlockAlign     = H.NumChannels * sizeof(float);
    H.ByteRate       = H.SampleRate * H.BlockAlign;
    H.Subchunk2Size  = NumFloatBytes;
    H.ChunkSize      = 36 + H.Subchunk2Size;

    FBufferArchive Ar;
    Ar.Serialize(&H, sizeof(FWavHeader));
    Ar.Serialize((void*)Samples.GetData(), NumFloatBytes);

    bool bOK = FFileHelper::SaveArrayToFile(Ar, *Filename);

    Ar.FlushCache();
    Ar.Empty();

    return bOK;
}
