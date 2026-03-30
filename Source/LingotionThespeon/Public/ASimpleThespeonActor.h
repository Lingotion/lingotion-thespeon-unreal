// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "Utils/AudioStreamComponent.h"
#include "ASimpleThespeonActor.generated.h"

class UThespeonComponent;

/**
 * A simple ready-to-use actor with a built-in UThespeonComponent for quick speech synthesis.
 *
 * Drop this actor into a level and configure its test properties in the Details panel
 * to quickly test text-to-speech without creating a custom actor. If bAutoSynthesizeOnBeginPlay
 * is enabled, synthesis starts automatically when the game begins.
 */
UCLASS()
class LINGOTIONTHESPEON_API ASimpleThespeonActor : public AActor
{
	GENERATED_BODY()

  public:
	ASimpleThespeonActor();

	UFUNCTION()
	void OnAudioReceived(FString SessionID, const TArray<float>& SynthData);

  protected:
	void BeginPlay() override;

  private:
	/** The Thespeon component that handles speech synthesis. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UThespeonComponent> ThespeonComponent;

	/** The audio stream component that plays the synthesized audio. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioStreamComponent> AudioStreamComponent;

	/** Name of the character voice to use for test synthesis. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	FString TestCharacterName = TEXT("DefaultCharacter");

	/** Quality tier of the module to use for test synthesis. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	EThespeonModuleType TestModuleType;

	/** Language to use for test synthesis. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	FLingotionLanguage TestLanguage;

	/** Text content to synthesize during testing. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	FString TestTextToSynthesize = TEXT("Hello World");

	/** When true, automatically starts synthesis using the test config when BeginPlay is called. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	bool bAutoSynthesizeOnBeginPlay = false;
};
