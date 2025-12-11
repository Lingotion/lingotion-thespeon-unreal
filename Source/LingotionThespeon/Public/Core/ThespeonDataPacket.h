// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Core/ModelInput.h"
#include "Misc/TVariant.h"


namespace Thespeon
{
    namespace Core
    {
        struct FPacketMetadata
        {
            FString SessionID;
            FString CharacterName;
            EThespeonModuleType ModuleType;
            // TQueue<int> RequestedAudioIndices;
            bool bPacketStatus;
            FPacketMetadata() = default;
            FPacketMetadata(FString SessionID, FString CharacterName = TEXT(""), EThespeonModuleType ModuleType = EThespeonModuleType::None, bool bPacketStatus = false/*, TQueue<int> requestedAudioIndices = TQueue<int>()*/);
        };
        template<typename T>
        struct FThespeonDataPacket
        {
            TArray<T> Data;
            bool bIsFinalPacket;
            FPacketMetadata Metadata;
            FThespeonDataPacket() = default;
        };
        using FAnyPacket = TVariant<FThespeonDataPacket<float> /*, Thespeon::Core::FThespeonDataPacket<int>, etc*/>; //We will try this construction out and see how well it works.
    }
}
