// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

namespace Thespeon
{
namespace Inference
{
/**
 * Per-synthesis usage statistics for the editor.
 *
 * Keyed by character module ID; each inner map holds string keys (nbrSynths, lowercased emotion
 * names, and the blend-cardinality buckets blend1/blend2/blend3plus) to floating-point values.
 * nbrSynths is 1; the emotion and blend values count characters of text, fractionally for blends.
 */
struct FThespeonDataCache
{
	TMap<FString, TMap<FString, double>> Data;
};

/**
 * Broadcast once per (non-warmup) synthesis with that synthesis's data cache. It is broadcast before
 * inference runs, so syntheses that later fail or are cancelled are still counted.
 *
 * Always broadcast on the game thread. The editor module owns the single listener.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSynthesisDataSignal, const FThespeonDataCache& /*DataCache*/);

/**
 * Holder for the process-wide synthesis-data signal. Runtime code broadcasts; editor code binds.
 */
struct LINGOTIONTHESPEON_API FThespeonEditorSignals
{
	static FOnSynthesisDataSignal OnSynthesisDataSignal;
};

} // namespace Inference
} // namespace Thespeon

#endif // WITH_EDITOR
