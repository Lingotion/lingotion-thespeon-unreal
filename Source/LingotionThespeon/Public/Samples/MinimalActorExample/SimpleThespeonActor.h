// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "Utils/AudioStreamComponent.h"
#include "SimpleThespeonActor.generated.h"

class UThespeonComponent;

/**
 * A simple ready-to-use actor with a built-in UThespeonComponent for quick speech synthesis.
 *
 * Drop this actor into a level and configure its test properties in the Details panel
 * to quickly test text-to-speech without creating a custom actor. If bAutoSynthesizeOnBeginPlay
 * is enabled, synthesis starts about one second after the game begins.
 *
 * The emotion is fixed to Interest; build your own FLingotionModelInput to use other emotions.
 */
UCLASS()
class LINGOTIONTHESPEON_API ASimpleThespeonActor : public AActor
{
	GENERATED_BODY()

  public:
	ASimpleThespeonActor();

	/** Forwards received audio to AudioStreamComponent for playback. Bound in BeginPlay. */
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

	/** Name of the character to use for test synthesis. Must be set to an imported character name. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	FString TestCharacterName = TEXT("DefaultCharacter");

	/** Module quality tier (XL to XS) to use for test synthesis. None uses the project's default module type. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	EThespeonModuleType TestModuleType;

	/** Language to use for test synthesis. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	FLingotionLanguage TestLanguage;

	/** Text content to synthesize during testing. */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	FString TestTextToSynthesize = TEXT("Hello World");

	/** When true, synthesizes TestTextToSynthesize about one second after BeginPlay (skipped if the text is empty), with session ID
	 * "AutoConfigSession". */
	UPROPERTY(EditAnywhere, Category = "Thespeon Test Config")
	bool bAutoSynthesizeOnBeginPlay = false;
};
