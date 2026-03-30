// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "Utils/AudioStreamComponent.h"
#include "AAngelDevilDemoActor.generated.h"

class UThespeonComponent;

UENUM()
enum class EDemoStatus : int32
{
	Idle = 0,
	InProgress = 1,
	Success = 2,
	Failed = -1
};

/**
 * Per-role data: editable config and runtime state all in one place.
 * Index 0 = Person, 1 = Angel, 2 = Devil.
 *
 * Note: component pointers (UThespeonComponent, UAudioStreamComponent) are kept as direct
 * UPROPERTY TArrays on the actor — UE's instancing system only remaps subobject references
 * for top-level UPROPERTY fields, not for pointers nested inside a struct inside a TArray.
 */
USTRUCT()
struct FRoleEntry
{
	GENERATED_BODY()

	// --- Editable config ---

	UPROPERTY(EditAnywhere, Category = "Config")
	FString Character;

	UPROPERTY(EditAnywhere, Category = "Config")
	EEmotion Emotion = EEmotion::None;

	UPROPERTY(EditAnywhere, Category = "Config")
	FString Text;

	// --- Runtime state (not reflected; reset each session) ---

	EDemoStatus PreloadStatus = EDemoStatus::Idle;
	double PreloadElapsed = 0.0;
	EDemoStatus SynthStatus = EDemoStatus::Idle;
	double SynthElapsed = 0.0;
	int32 PacketsReceived = 0;
};

/**
 * Demo actor: "Angel and Devil on Your Shoulder"
 *
 * Three characters respond to a moral dilemma simultaneously, each with a different emotion:
 *   - Person (Aaron Archer)  — neutral, pondering the question
 *   - Angel  (Deryn Oliver)  — serene, giving kind advice
 *   - Devil  (Vladrus)       — angry, giving aggressive advice
 *
 * Drop into a level and press Play. All 3 characters preload concurrently.
 * Press 4 to trigger all 3 to speak at the same time with different emotions.
 *
 * Press 1/2/3 = synth Person / Angel / Devil individually
 * Press 4     = synth ALL simultaneously
 * Press 5     = cancel all
 */
UCLASS()
class LINGOTIONTHESPEON_API AAngelDevilDemoActor : public AActor
{
	GENERATED_BODY()

  public:
	AAngelDevilDemoActor();

  protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

  private:
	// ---- Components (direct UPROPERTY so UE's instancing system remaps subobject references correctly) ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UThespeonComponent>> Comps;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UAudioStreamComponent>> AudioComps;

	// ---- Per-role config and runtime state ----

	UPROPERTY(EditAnywhere, Category = "Demo Config")
	TArray<FRoleEntry> Roles;

	// ---- Shared config ----

	UPROPERTY(EditAnywhere, Category = "Demo Config")
	EThespeonModuleType ModuleType = EThespeonModuleType::M;

	UPROPERTY(EditAnywhere, Category = "Demo Config")
	FLingotionLanguage Language;

	UPROPERTY(EditAnywhere, Category = "Demo Config")
	bool bAutoSynthesizeOnPreloadComplete = false;

	// ---- Shared runtime state ----

	static constexpr int32 NumRoles = 3;

	double PreloadStartTime = 0.0;
	double SynthStartTime = 0.0;
	bool bConcurrentSynthActive = false;

	TMap<FString, int32> SessionToIndex;

	// ---- Role labels & colors ----

	static const FString RoleLabels[NumRoles];
	static const FColor RoleColors[NumRoles];

	// ---- Preload callbacks (per-index trampolines: no session ID, CharacterName not unique across roles) ----

	UFUNCTION()
	void OnPreloadComplete0(bool bSuccess, FString CharacterName, EThespeonModuleType CompModuleType, EBackendType BackendType);
	UFUNCTION()
	void OnPreloadComplete1(bool bSuccess, FString CharacterName, EThespeonModuleType CompModuleType, EBackendType BackendType);
	UFUNCTION()
	void OnPreloadComplete2(bool bSuccess, FString CharacterName, EThespeonModuleType CompModuleType, EBackendType BackendType);
	void HandlePreloadComplete(int32 Index, bool bSuccess, const FString& CharacterName);

	// ---- Audio & synthesis callbacks (single shared; index resolved via SessionToIndex) ----

	UFUNCTION()
	void OnAudioReceived(FString SessionID, const TArray<float>& SynthData);
	UFUNCTION()
	void OnSynthComplete(FString SessionID);
	UFUNCTION()
	void OnSynthFailed(FString SessionID);
	void HandleSynthComplete(int32 Index, const FString& SessionID, bool bSuccess);

	// ---- Helpers ----

	void SynthesizeRole(int32 Index);
	void SynthesizeAll();
	bool AllPreloadsComplete() const;
	bool AllSynthsComplete() const;

	static const FString GetPreloadString(EDemoStatus Status, double Elapsed, double TotalElapsed);
	static const FString GetSynthString(EDemoStatus Status, double Elapsed, double TotalElapsed, int32 PacketsReceived);
};
