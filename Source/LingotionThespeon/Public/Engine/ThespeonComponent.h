// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InferenceConfig.h"
#include "Core/ThespeonDataPacket.h"
#include "HAL/RunnableThread.h"
#include <atomic>
#include "ThespeonComponent.generated.h"

class UAudioStreamComponent;

// Forward declarations to avoid including private headers
namespace Thespeon
{
namespace Inference
{
class ThespeonInference;
class FPreloadSession;
} // namespace Inference
} // namespace Thespeon

// TUniquePtr with forward declared types needs custom deleters
// so the compiler can destroy them without requiring a complete type in the header.
struct FThespeonInferenceDeleter
{
	void operator()(Thespeon::Inference::ThespeonInference* Ptr) const;
};

struct FPreloadSessionDeleter
{
	void operator()(Thespeon::Inference::FPreloadSession* Ptr) const;
};

/** A single entry in a PreloadCharacterGroup call. */
USTRUCT(BlueprintType)
struct FPreloadEntry
{
	GENERATED_BODY()

	/** The name of the character to preload. Must exactly match an imported character. */
	UPROPERTY(BlueprintReadWrite, Category = "Lingotion Thespeon")
	FString CharacterName;

	/** The module type to preload. Must exactly match an imported module of the character. */
	UPROPERTY(BlueprintReadWrite, Category = "Lingotion Thespeon")
	EThespeonModuleType ModuleType = EThespeonModuleType::None;

	/**
	 * Optional: selects the backend to load on. Only BackendType and bForceRequestedBackend are used for preloading.
	 * Duplicate detection ignores bForceRequestedBackend, so a request differing only in that flag is dropped.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Lingotion Thespeon")
	FInferenceConfig InferenceConfig;
};

/**
 * Signature of UThespeonComponent::OnAudioReceived, broadcast on the game thread whenever a chunk of
 * synthesized audio is ready. The component does not play audio itself; pass the samples to e.g.
 * UAudioStreamComponent::SubmitAudioToStream for playback.
 *
 * @param SessionID The session ID passed to Synthesize.
 * @param SynthesisData Mono 32-bit float samples at 44100 Hz.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioReceived, FString, SessionID, const TArray<float>&, SynthesisData);
/** Signature of UThespeonComponent::OnAudioSampleRequestReceived: the session ID and the sample indices of the AudioSampleRequest characters. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioSampleRequestReceived, FString, SessionID, const TArray<int64>&, TriggerAudioSamples);
/** Signature of UThespeonComponent::OnSynthesisComplete: the session ID of the finished synthesis. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisComplete, FString, SessionID);
/** Signature of UThespeonComponent::OnPreloadComplete: whether the preload succeeded, and the character, module type and backend it was for. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FOnPreloadComplete, bool, PreloadSuccess, FString, CharacterName, EThespeonModuleType, ModuleType, EBackendType, BackendType
);
/** Signature of UThespeonComponent::OnPreloadGroupComplete: the group ID and whether every preload in the group succeeded. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPreloadGroupComplete, FString, PreloadGroupId, bool, bAllSucceeded);
/** Signature of UThespeonComponent::OnSynthesisFailed: the session ID of the failed or rejected synthesis. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisFailed, FString, SessionID);

/** Main component needed to use Lingotion Thespeon. Needs to be added to an Actor; if the Actor has no root component, one is created on register. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LINGOTIONTHESPEON_API UThespeonComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UThespeonComponent();
	~UThespeonComponent() override; // Destructor needed for TUniquePtr with forward declared type
	void OnRegister() override;

	/**
	 * @brief Schedules an audio generation job. Requests are queued and run one at a time per component.
	 * Characters that have not been preloaded are loaded before audio generation starts.
	 * An unknown CharacterName or unavailable ModuleType falls back to an imported one, with a warning.
	 * Bind delegates before calling: a rejected request may broadcast OnSynthesisFailed before this returns.
	 *
	 * @param Input The FLingotionModelInput to use.
	 * @param SessionId Optional: An identifier for this specific task.
	 * @param InferenceConfig Optional: A specific configuration to take into account during synthesis.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void Synthesize(FLingotionModelInput Input, FString SessionId = TEXT(""), FInferenceConfig InferenceConfig = FInferenceConfig());

	/**
	 * @brief Attempts to load the files for a specific character and module type into memory.
	 *        Fires OnPreloadComplete when done. For grouped preloads use PreloadCharacterGroup.
	 *        The name and module type must match an imported module exactly (no fallback; None always fails).
	 *        A call that duplicates a queued preload is ignored. Queued preloads wait while a synthesis is running or
	 *        queued, and at most 4 preloads run at once. If a queued synthesis needs the same model, the preload is
	 *        folded into it and OnPreloadComplete is not broadcast.
	 *
	 * @param CharacterName The name of the character.
	 * @param ModuleType The specific module type to load.
	 * @param InferenceConfig Optional: A specific configuration to take into account when loading.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void PreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());

	/**
	 * @brief Preloads all entries as one group.
	 *        All entries are registered before any preload starts, so OnPreloadGroupComplete
	 *        cannot fire early, however fast individual preloads complete.
	 *        Prefer this over calling PreloadCharacter in a loop when you need to know when all of them are done.
	 *        Requires a non-empty PreloadGroupId. Entries that duplicate queued requests are not counted, and if none
	 *        remain, OnPreloadGroupComplete is not broadcast.
	 *
	 * @param Characters The list of characters and module types to preload.
	 * @param PreloadGroupId Identifier for the group. OnPreloadGroupComplete fires once when all are done.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void PreloadCharacterGroup(TArray<FPreloadEntry> Characters, FString PreloadGroupId);

	/**
	 * @brief Checks whether a character of the given module type is fully loaded on the backend
	 *        the given config resolves to, whether it was loaded by a preload or on demand by Synthesize.
	 *        Fully loaded means the character module and every imported language module it uses are loaded,
	 *        so this stays false until a running preload has finished loading everything.
	 *
	 * @param CharacterName The name of the character.
	 * @param ModuleType The specific module type to check.
	 * @param InferenceConfig Optional: the configuration whose backend the check applies to.
	 * @return True if the character and its language modules are loaded on that backend.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	bool IsLoaded(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());

	/**
	 * @brief Attempts to unload a character of the given module type on the provided backend.
	 * If no backend is provided, unload will be attempted for all loaded backends.
	 * Unload fails if no loaded character matches the specified combination of character name, module type, and backend type.
	 * Runs synchronously on the calling thread and also unloads language modules no other loaded character uses.
	 * Does not check for a running synthesis or preload of the character.
	 *
	 * @param CharacterName The name of the character.
	 * @param ModuleType The specific module type to unload.
	 * @param BackendType The specific backend type to unload. Use EBackendType::None (default) to unload all backends.
	 * @return True if unloading succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	bool TryUnloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, EBackendType BackendType = EBackendType::None);
	void OnUnregister() override;
	void BeginDestroy() override;

	/** @brief Called whenever a chunk of synthesized audio is ready. The component does not play audio itself:
	 * bind this and pass the samples to e.g. a UAudioStreamComponent. Broadcast on the game thread. Not called for warmup sessions.
	 * @param SessionID The session that the data comes from.
	 * @param SynthesisData Mono 32-bit float samples at 44100 Hz.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnAudioReceived OnAudioReceived;

	/** @brief Called when the sample indices for the AudioSampleRequest control characters in the input are ready.
	 * Combined with OnAudioReceived, these can be used to trigger events when a specific word is spoken.
	 * Broadcast on the game thread, also for warmup sessions.
	 * @param SessionID The session that the data comes from.
	 * @param TriggerAudioSamples The audio sample indices of the AudioSampleRequest characters, in left-to-right order.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnAudioSampleRequestReceived OnAudioSampleRequestReceived;

	/** @brief Called after the final audio packet of a session has been delivered. Broadcast on the game thread, also for
	 * warmup sessions, after IsSynthesizing has turned false and before the next queued request starts.
	 * @param SessionID The session that the data comes from.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnSynthesisComplete OnSynthesisComplete;

	/** @brief Called when an individual preload is complete.
	 * @param PreloadSuccess True if preload succeeded. False otherwise.
	 * @param CharacterName The name of the character that was attempted to be preloaded.
	 * @param ModuleType The module type that was attempted to be preloaded.
	 * @param BackendType The backend type that was attempted to be preloaded.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnPreloadComplete OnPreloadComplete;

	/** @brief Called when all preloads in a group are complete.
	 * @param PreloadGroupId The group ID passed to PreloadCharacterGroup.
	 * @param bAllSucceeded True if every preload in the group succeeded.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnPreloadGroupComplete OnPreloadGroupComplete;

	/** @brief Called on the game thread when synthesis fails, or synchronously from Synthesize or queue processing when
	 * a request is rejected before it starts (no segments, or no imported character or module to fall back to).
	 * Not called on CancelSynthesis, or for requests still queued when the component unregisters.
	 * @param SessionID The session ID of the failed synthesis.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnSynthesisFailed OnSynthesisFailed;

	/**
	 * @brief Cancels the running synthesis session, if any. Blocks until the worker thread exits, then starts the next
	 * queued request. Queued requests are not cleared, and no delegate is broadcast. Audio already broadcast is not
	 * cleared; call UAudioStreamComponent::ResetBuffer to stop audio already queued for playback.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void CancelSynthesis();

	/** @brief Returns true if a synthesis session is currently running. Stays false while a request is only queued. */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	bool IsSynthesizing() const
	{
		return bIsSynthesizing.load();
	}

  private:
	struct PreloadRequest
	{
		FString CharacterName;
		EThespeonModuleType ModuleType;
		EBackendType BackendType;
		bool bForceRequestedBackend = false;
		FString PreloadGroupId;

		// PreloadGroupId and bForceRequestedBackend intentionally excluded — dedup is by character/module/backend only.
		bool operator==(const PreloadRequest& Other) const
		{
			return CharacterName == Other.CharacterName && ModuleType == Other.ModuleType && BackendType == Other.BackendType;
		}
	};

	struct SynthRequest
	{
		FLingotionModelInput Input;
		FString SessionId;
		FInferenceConfig InferenceConfig;
	};

	void PruneFinishedPreloadThreads();
	void ProcessPendingRequests();
	/** Returns true if a currently active preload is loading the model the next queued synth needs. */
	bool IsActivePreloadBlockingNextSynth() const;
	/** Starts a synthesis session. Returns false (and broadcasts OnSynthesisFailed) if the request was rejected. */
	bool RunSynthesisRequest(SynthRequest Request);
	void RunPreloadRequest(PreloadRequest Request);
	void ResetState();
	void CleanupThread();
	/** Stops and joins all preload threads and clears preload and group state. Safe to call multiple times. */
	void CleanupPreloadThreads();
	EThreadPriority ConvertToNativeThreadPriority(EThreadPriorityWrapper Wrapper) const;
	void PacketHandler(const FString& SessionID, const Thespeon::Core::FThespeonDataPacket& Packet);
	TQueue<TArray<float>> AudioDataQueue;
	TQueue<SynthRequest> SynthRequestQueue;
	TArray<PreloadRequest> PreloadRequests;
	std::atomic<int32> CurrentDataLength{0}; // Buffered sample count (floats) awaiting the next OnAudioReceived flush
	std::atomic<bool> bIsSynthesizing{false};
	std::atomic<int32> ActivePreloadCount{0};
	static constexpr int32 MaxConcurrentPreloads = 4;

	// Synthesis thread and session
	TUniquePtr<FRunnableThread> SessionThread;
	TUniquePtr<Thespeon::Inference::ThespeonInference, FThespeonInferenceDeleter> Session;

	// Preload threads and sessions — supports multiple parallel preloads.
	// All threads are joined via CleanupPreloadThreads() for deterministic teardown in OnUnregister.
	TArray<TUniquePtr<FRunnableThread>> PreloadThreads;
	TArray<TUniquePtr<Thespeon::Inference::FPreloadSession, FPreloadSessionDeleter>> ActivePreloadSessions;

	// Per-group tracking for OnPreloadGroupComplete.
	TMap<FString, int32> PreloadGroupPendingCounts;
	TMap<FString, bool> PreloadGroupHadFailure;
};
