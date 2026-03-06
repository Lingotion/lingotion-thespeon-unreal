// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InferenceConfig.h"
#include "Core/ThespeonDataPacket.h"
#include "ThespeonComponent.generated.h"

class UAudioStreamComponent;

// Forward declaration to avoid including private header
namespace Thespeon
{
namespace Inference
{
class ThespeonInference;
}
} // namespace Thespeon

// TUniquePtr with forward declared type needs to be paired with a destructor
// for the compiler to consider it complete
struct FThespeonInferenceDeleter
{
	void operator()(Thespeon::Inference::ThespeonInference* Ptr) const;
};

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
	 *
	 * @param CharacterName The name of the character.
	 * @param ModuleType The specific module type to load.
	 * @param InferenceConfig Optional: A specific configuration to take into account when loading.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon")
	void PreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());

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

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioReceived, FString, SessionID, const TArray<float>&, SynthesisData);
	/** @brief Called whenever synthesized audio is available. Replaces basic audio playback if bound.
	 * @param SessionID The Session that the data comes from.
	 * @param SynthesisData The audio data as an array of floats.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnAudioReceived OnAudioReceived;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioSampleRequestReceived, FString, SessionID, const TArray<int64>&, TriggerAudioSamples);
	/** @brief Called when the sample indices corresponding to the trigger input character are ready.
	 * Combined with FOnAudioReceived, these can be used to trigger specific events when a specific word is spoken.
	 * @param SessionID The Session that the data comes from.
	 * @param TriggerAudioSamples The ordered audio samples corresponding to the trigger input characters.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnAudioSampleRequestReceived OnAudioSampleRequestReceived;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisComplete, FString, SessionID);
	/** @brief Called when final packet is delivered to caller.
	 * @param SessionID The Session that the data comes from.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnSynthesisComplete OnSynthesisComplete;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	    FOnPreloadComplete, bool, PreloadSuccess, FString, CharacterName, EThespeonModuleType, ModuleType, EBackendType, BackendType
	);
	/** @brief Called when preload is complete.
	 * @param PreloadSuccess True if preload succeeded. False otherwise.
	 * @param CharacterName The name of the character that was attempted to be preloaded.
	 * @param ModuleType The module type that was attempted to be preloaded.
	 * @param BackendType The backend type that was attempted to be preloaded.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Audio")
	FOnPreloadComplete OnPreloadComplete;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSynthesisFailed, FString, SessionID);
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
		return bIsSynthesizing;
	}

  private:
	struct PreloadRequest
	{
		FString CharacterName;
		EThespeonModuleType ModuleType;
		EBackendType BackendType;

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
	UAudioStreamComponent* StreamComp; // keep it from GC. TODO: REMOVE BEFORE RELEASE?
	void ProcessPendingRequests();
	void RunSynthesisRequest(SynthRequest Request);
	void RunPreloadRequest(PreloadRequest Request);
	void CleanupThread();
	EThreadPriority ConvertToNativeThreadPriority(EThreadPriorityWrapper Wrapper) const;
	void PacketHandler(const FString& SessionID, const Thespeon::Core::FThespeonDataPacket& Packet);
	TQueue<TArray<float>> AudioDataQueue;
	TQueue<SynthRequest> SynthRequestQueue;
	TArray<PreloadRequest> PreloadRequests;
	int CurrentDataLength = 0; // in number of T (elements of ThespeonDataPacket TArray)
	bool bIsSynthesizing = false;
	bool bIsPreloading = false;
	FRunnableThread* SessionThread = nullptr;
	TUniquePtr<Thespeon::Inference::ThespeonInference, FThespeonInferenceDeleter> Session;
};
