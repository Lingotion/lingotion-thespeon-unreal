// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "ASimpleThespeonActor.h"
#include "Engine/ThespeonComponent.h"
#include "Core/LingotionLogger.h"
#include "Core/ModelInput.h"
#include "TimerManager.h"

// Sets default values
ASimpleThespeonActor::ASimpleThespeonActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create and initialize the Thespeon component
	ThespeonComponent = CreateDefaultSubobject<UThespeonComponent>(TEXT("ThespeonComponent"));
	AudioStreamComponent = CreateDefaultSubobject<UAudioStreamComponent>(TEXT("AudioStreamComponent"));
}

void ASimpleThespeonActor::OnAudioReceived(FString SessionID, const TArray<float>& SynthData)
{
	if (AudioStreamComponent)
	{
		AudioStreamComponent->SubmitAudioToStream(SynthData);
	}
}

// Called when the game starts or when spawned
void ASimpleThespeonActor::BeginPlay()
{
	Super::BeginPlay();

	// Start the audio stream component playback
	AudioStreamComponent->Start();

	// Bind OnAudioReceived
	ThespeonComponent->OnAudioReceived.AddDynamic(this, &ASimpleThespeonActor::OnAudioReceived);

	// Auto-synthesize if enabled in Details panel
	if (bAutoSynthesizeOnBeginPlay && !TestTextToSynthesize.IsEmpty())
	{
		FLingotionModelInput Input;
		Input.CharacterName = TestCharacterName;
		Input.ModuleType = TestModuleType;
		Input.DefaultEmotion = EEmotion::Interest;
		Input.DefaultLanguage = TestLanguage; // Set the language from Details panel

		FLingotionInputSegment Segment;
		Segment.Text = TestTextToSynthesize;
		Segment.Language = TestLanguage; // Also set it on the segment
		Input.Segments.Add(Segment);

		// Call after delay
		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(
		    TimerHandle,
		    [this, Input]()
		    {
			    LINGO_LOG(EVerbosityLevel::Info, TEXT("ASimpleThespeonActor Auto-synthesizing from Details panel Input"));
			    if (ThespeonComponent)
			    {
				    ThespeonComponent->Synthesize(Input, TEXT("AutoConfigSession"));
			    }
			    else
			    {
				    LINGO_LOG(EVerbosityLevel::Error, TEXT("ASimpleThespeonActor ThespeonComponent is null! Make sure it is properly initialized."));
			    }
		    },
		    1.0f,
		    false
		);
	}
}