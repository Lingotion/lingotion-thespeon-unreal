// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InferenceConfig.h"
#include "Core/ThespeonDataPacket.h"
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

	UPROPERTY(BlueprintReadWrite, Category = "Lingotion Thespeon")
	FString CharacterName;

	UPROPERTY(BlueprintReadWrite, Category = "Lingotion Thespeon")
	EThespeonModuleType ModuleType = EThespeonModuleType::None;

	UPROPERTY(BlueprintReadWrite, Category = "Lingotion Thespeon")
	FInferenceConfig InferenceConfig;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioReceived, FString, SessionID, const TArray<float>&, SynthesisData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioSampleRequestReceived, FString, SessionID, const TArray<int64>&, TriggerAudioSamples);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisComplete, FString, SessionID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FOnPreloadComplete, bool, PreloadSuccess, FString, CharacterName, EThespeonModuleType, ModuleType, EBackendType, BackendType
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPreloadGroupComplete, FString, PreloadGroupId, bool, bAllSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisFailed, FString, SessionID);

/** Main component needed to use Lingotion Thespeon. Needs to be added to an Actor. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LINGOTIONTHESPEON_API UThespeonComponent : public UActorComponent
{
	GENERATED_BODY()
  public:
	UThespeonComponent();
	~UThespeonComponent() override; // Destructor needed for TUniquePtr with forward declared type
	void OnRegister() override;

	/**
	 * @brief Schedules an audio generation job. Characters that have not been preloaded will be loaded before audio generation starts.
	 *
	 * @param Input The LingotionModelInput to use.
	 * @param SessionId Optional: An identifier for this specific task.
	 * @param InferenceConfig Optional: A specific configuration to take into account during synthesis.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void Synthesize(FLingotionModelInput Input, FString SessionId = TEXT(""), FInferenceConfig InferenceConfig = FInferenceConfig());

	/**
	 * @brief Attempts to load the files for a specific character and module type into memory.
	 *        Fires OnPreloadComplete when done. For grouped preloads use PreloadCharacterGroup.
	 *
	 * @param CharacterName The name of the character.
	 * @param ModuleType The specific module type to load.
	 * @param InferenceConfig Optional: A specific configuration to take into account when loading.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void PreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());

	/**
	 * @brief Preloads all entries in a single atomic group.
	 *        All counts are registered before any thread starts, so OnPreloadGroupComplete
	 *        cannot fire prematurely regardless of how fast individual preloads complete.
	 *        Prefer this over calling PreloadCharacter in a loop with a shared group ID.
	 *
	 * @param Characters The list of characters and module types to preload.
	 * @param PreloadGroupId Identifier for the group. OnPreloadGroupComplete fires once when all are done.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void PreloadCharacterGroup(TArray<FPreloadEntry> Characters, FString PreloadGroupId);

	/**
	 * @brief Attempts to unload a character of the given module type on the provided backend.
	 * If no backend is provided, unload will be attempted for all loaded backends.
	 * Unload fails if no loaded character matches the specified combination of character name, module type, and backend type.
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

	/** @brief Called whenever synthesized audio is available. Replaces basic audio playback if bound.
	 * @param SessionID The Session that the data comes from.
	 * @param SynthesisData The audio data as an array of floats.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnAudioReceived OnAudioReceived;

	/** @brief Called when the sample indices corresponding to the trigger input character are ready.
	 * Combined with FOnAudioReceived, these can be used to trigger specific events when a specific word is spoken.
	 * @param SessionID The Session that the data comes from.
	 * @param TriggerAudioSamples The ordered audio samples corresponding to the trigger input characters.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnAudioSampleRequestReceived OnAudioSampleRequestReceived;

	/** @brief Called when final packet is delivered to caller.
	 * @param SessionID The Session that the data comes from.
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
	 * @param PreloadGroupId The group ID passed to PreloadCharacter calls.
	 * @param bAllSucceeded True if every preload in the group succeeded.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnPreloadGroupComplete OnPreloadGroupComplete;

	/** @brief Called when synthesis has failed.
	 * @param SessionID The session ID of the failed synthesis.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnSynthesisFailed OnSynthesisFailed;

	/**
	 * @brief Cancels the current synthesis session if one is running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void CancelSynthesis();

	/** @brief Returns true if a synthesis session is currently running. */
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
		FString PreloadGroupId;

		// PreloadGroupId intentionally excluded — dedup is by character/module/backend only.
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
	void RunSynthesisRequest(SynthRequest Request);
	void RunPreloadRequest(PreloadRequest Request);
	void ResetState();
	void CleanupThread();
	/** Joins and destroys the preload thread. Safe to call multiple times. */
	void CleanupPreloadThreads();
	EThreadPriority ConvertToNativeThreadPriority(EThreadPriorityWrapper Wrapper) const;
	void PacketHandler(const FString& SessionID, const Thespeon::Core::FThespeonDataPacket& Packet);
	TQueue<TArray<float>> AudioDataQueue;
	TQueue<SynthRequest> SynthRequestQueue;
	TArray<PreloadRequest> PreloadRequests;
	std::atomic<int32> CurrentDataLength{0}; // in number of T (elements of ThespeonDataPacket TArray)
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
