// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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