// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "ASimpleThespeonActor.generated.h"

class UThespeonComponent;

UCLASS()
class LINGOTIONTHESPEON_API ASimpleThespeonActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASimpleThespeonActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Get the Thespeon component
	UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
	UThespeonComponent* GetThespeonComponent() const { return ThespeonComponent; }

	// Wrapper methods to access ThespeonComponent functionality
	UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
	void Synthesize(FLingotionModelInput Input, FString SessionId = TEXT(""), FInferenceConfig InferenceConfig = FInferenceConfig());

	UFUNCTION(BlueprintCallable, Category="Lingotion Thespeon")
	bool TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig = FInferenceConfig());

private:
	// The Thespeon component that handles speech synthesis
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lingotion Thespeon", meta=(AllowPrivateAccess="true"))
	UThespeonComponent* ThespeonComponent;

	// Editable properties for testing
	UPROPERTY(EditAnywhere, Category="Thespeon Test Config")
	FString TestCharacterName = TEXT("DefaultCharacter");
	
	UPROPERTY(EditAnywhere, Category="Thespeon Test Config")
	EThespeonModuleType TestModuleType;
	
	UPROPERTY(EditAnywhere, Category="Thespeon Test Config")
	FLingotionLanguage TestLanguage;
	
	UPROPERTY(EditAnywhere, Category="Thespeon Test Config")
	FString TestTextToSynthesize = TEXT("Hello World");
	
	UPROPERTY(EditAnywhere, Category="Thespeon Test Config")
	bool bAutoSynthesizeOnBeginPlay = false;
};
