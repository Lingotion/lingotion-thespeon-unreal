// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

namespace Thespeon
{
namespace Inference
{
/**
 *
 * Keyed by character module ID; each inner map holds string keys (nbrSynths, lowercased emotion
 * names, and the blend-cardinality buckets blend1/blend2/blend3plus) to floating-point counts.
 * Counts are stored as doubles because per-emotion marginals accumulate fractionally.
 */
struct FThespeonDataCache
{
	TMap<FString, TMap<FString, double>> Data;
};

/**
 * Broadcast once per (non-warmup) synthesis with that synthesis's data cache.
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
