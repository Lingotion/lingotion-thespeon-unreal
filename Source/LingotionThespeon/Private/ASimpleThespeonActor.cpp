// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "ASimpleThespeonActor.h"
#include "Engine/ThespeonComponent.h"
#include "Core/LingotionLogger.h"
#include "Core/ModelInput.h"
#include "TimerManager.h"

// Sets default values
ASimpleThespeonActor::ASimpleThespeonActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create and initialize the Thespeon component
	ThespeonComponent = CreateDefaultSubobject<UThespeonComponent>(TEXT("ThespeonComponent"));
}

// Called when the game starts or when spawned
void ASimpleThespeonActor::BeginPlay()
{
	Super::BeginPlay();
	
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
		GetWorldTimerManager().SetTimer(TimerHandle, [this, Input]()
		{
			LINGO_LOG(EVerbosityLevel::Warning, TEXT("MyActor: Auto-synthesizing from Details panel config"));
			Synthesize(Input, TEXT("AutoConfigSession"));
		}, 1.0f, false);
	}
}

// Called every frame
void ASimpleThespeonActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASimpleThespeonActor::Synthesize(FLingotionModelInput Input, FString SessionId, FInferenceConfig InferenceConfig)
{
	if (ThespeonComponent)
	{
		ThespeonComponent->Synthesize(Input, SessionId, InferenceConfig);
	}
	else
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("MyActor::Synthesize - ThespeonComponent is null!"));
	}
}

bool ASimpleThespeonActor::TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig)
{
	if (ThespeonComponent)
	{
		return ThespeonComponent->TryPreloadCharacter(CharacterName, ModuleType, InferenceConfig);
	}
	
	LINGO_LOG(EVerbosityLevel::Error, TEXT("MyActor::TryPreloadCharacter - ThespeonComponent is null!"));
	return false;
}
