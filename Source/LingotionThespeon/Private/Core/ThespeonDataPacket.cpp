// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Core/ThespeonDataPacket.h"

Thespeon::Core::FPacketMetadata::FPacketMetadata(FString SessionID, FString CharacterName, EThespeonModuleType ModuleType, bool bPacketStatus/*, TQueue<int> requestedAudioIndices*/)
    : SessionID(SessionID), CharacterName(CharacterName), ModuleType(ModuleType), bPacketStatus(bPacketStatus) //, RequestedAudioIndices(requestedAudioIndices)
{
}
