// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InferenceConfig.h"
#include "Core/ThespeonDataPacket.h"
#include "ThespeonComponent.generated.h"

class UAudioStreamComponent;

// Forward declaration to avoid including private header
namespace Thespeon { namespace Inference { class ThespeonInference; } }

// TUniquePtr with forward declared type needs to be paired with a destructor
// for the compiler to consider it complete
struct FThespeonInferenceDeleter
{
    void operator()(Thespeon::Inference::ThespeonInference* Ptr) const;
};

/** Main component needed to use Lingotion Thespeon. Needs to be added to an Actor. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LINGOTIONTHESPEON_API UThespeonComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UThespeonComponent();
    virtual ~UThespeonComponent(); // Destructor needed for TUniquePtr with forward declared type
    virtual void OnRegister() override;
    
    /**
    * Schedules an audio generation job. Characters that have not been preloaded will be loaded before audio generation starts.
    *
    * @param Input The LingotionModelInput to use.
    * @param SessionId Optional: An identifier for this specific task.
    * @param InferenceConfig Optional: A specific configuration to take into account during synthesis.
    */
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
    void Synthesize(FLingotionModelInput Input, FString SessionId = TEXT(""), FInferenceConfig InferenceConfig = FInferenceConfig());
    
    /**
    * Attempts to load the files for a specific character and module type into memory.
    *
    * @param CharacterName The name of the character.
    * @param ModuleType The specific module type to load.
    * @param InferenceConfig Optional: A specific configuration to take into account when loading.
    * @return True if loading succeeded.
    */
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
    bool TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());
    
    /**
    * Attempts to purge the data for a specific character and module type from memory.
    *
    * @param CharacterName The name of the character.
    * @param ModuleType The specific module type to unload.
    * @return True if loading succeeded.
    */
    UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
    bool TryUnloadCharacter(FString CharacterName, EThespeonModuleType ModuleType);
    virtual void OnUnregister() override;
    virtual void BeginDestroy() override;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioReceived, const TArray<float>&, AudioData);
    /** Called whenever synthesized audio is available. Replaces basic audio playback if bound. */
    UPROPERTY(BlueprintAssignable, Category="Lingotion Thespeon|Audio")
    FOnAudioReceived OnAudioReceived;

private:
    UAudioStreamComponent* StreamComp; // keep it from GC REMOVE BEFORE RELEASE
    TQueue<Thespeon::Core::FThespeonDataPacket<float>> PacketQueue;
    void CleanupThread();
    void PacketHandler(const Thespeon::Core::FAnyPacket& packet, float BufferSeconds=0.5f);
    int CurrentDataLength = 0; // in number of T (elements of ThespeonDataPacket TArray)
    bool bIsSynthesizing = false;
    FRunnableThread* SessionThread = nullptr;
    TUniquePtr<
        Thespeon::Inference::ThespeonInference,
        FThespeonInferenceDeleter
    > Session;
};
