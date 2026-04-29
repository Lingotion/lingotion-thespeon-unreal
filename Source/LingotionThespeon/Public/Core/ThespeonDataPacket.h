// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Core/ModelInput.h"
#include "Misc/TVariant.h"

namespace Thespeon
{
namespace Core
{
/**
 * Defines the type of data carried by a synthesis callback packet.
 *
 * Must match the callback types defined in the meta graph protocol.
 */
enum class SynthCallbackType : uint8
{
	CB_UNSPECIFIED = 0,
	CB_ERROR = 1,
	CB_AUDIO = 2,
	CB_TRIGGERSAMPLE = 3
};

/**
 * Type alias for metadata values attached to data packets.
 *
 * A variant type that can hold any of the scalar types supported by
 * the meta graph protocol: int64, float, bool, or FString.
 */
using FPacketMetadataValue = TVariant<int64, float, bool, FString>;

/**
 * Type alias for the metadata map carried by data packets.
 *
 * Maps string keys to variant metadata values, allowing arbitrary
 * key-value metadata to be attached to synthesis results.
 */
using FMetadataMap = TMap<FString, FPacketMetadataValue>;

/**
 * Type alias for the payload data carried by data packets.
 *
 * A variant type that holds either an array of floats (audio sample data)
 * or an array of int64 values (trigger sample indices).
 */
using FPacketPayload = TVariant<TArray<float>, TArray<int64>>;

/**
 * Carries synthesis results between the inference thread and the game thread.
 *
 * Each packet contains a callback type indicating the kind of data,
 * a payload with the actual audio or trigger data, and an optional
 * metadata map with additional information about the packet.
 */
struct FThespeonDataPacket
{
	/** The type of callback this packet represents (audio, error, trigger, etc.). */
	SynthCallbackType CallbackType;

	/** The data payload, either audio samples (float array) or trigger indices (int64 array). */
	FPacketPayload Payload;

	/** Optional metadata key-value pairs attached to this packet. */
	FMetadataMap Metadata;

	/** Default constructor is deleted; a callback type must always be specified. */
	FThespeonDataPacket() = delete;

	/**
	 * Constructs a data packet with the specified callback type.
	 *
	 * @param InCallbackType The type of synthesis callback this packet represents.
	 */
	FThespeonDataPacket(SynthCallbackType InCallbackType) : CallbackType(InCallbackType) {}
};

} // namespace Core
} // namespace Thespeon
