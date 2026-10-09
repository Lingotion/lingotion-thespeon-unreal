// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "Utils/AudioStreamComponent.h"
#include "AngelDevilDemoActor.generated.h"

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

	/** Imported character name that voices this role. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FString Character;

	/** Emotion applied to the whole line. */
	UPROPERTY(EditAnywhere, Category = "Config")
	EEmotion Emotion = EEmotion::None;

	/** The line this role speaks. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FString Text;

	// --- Runtime state (not reflected): preload fields are set in BeginPlay, synth fields reset when the role starts speaking ---

	EDemoStatus PreloadStatus = EDemoStatus::Idle;
	double PreloadElapsed = 0.0;
	EDemoStatus SynthStatus = EDemoStatus::Idle;
	double SynthElapsed = 0.0;
	int32 PacketsReceived = 0;
};

/**
 * Demo actor: "Angel and Devil on Your Shoulder"
 *
 * Three roles respond to a moral dilemma simultaneously, each with a different emotion:
 *   - Person — Interest, asking the question
 *   - Angel  — Serenity, giving kind advice
 *   - Devil  — Anger, giving aggressive advice
 *
 * By default all three roles use Aaron Archer. Set each role's Character under
 * Demo Config > Roles in the Details panel to hear three different voices.
 *
 * Drop into a level and press Play. All 3 roles preload concurrently.
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

	/** Exactly 3 entries, in the order Person, Angel, Devil. Do not add or remove entries. */
	UPROPERTY(EditAnywhere, Category = "Demo Config")
	TArray<FRoleEntry> Roles;

	// ---- Shared config ----

	/** Module quality tier used by all three roles. */
	UPROPERTY(EditAnywhere, Category = "Demo Config")
	EThespeonModuleType ModuleType = EThespeonModuleType::M;

	/** Language used by all roles. If left empty, English ("eng") is used. */
	UPROPERTY(EditAnywhere, Category = "Demo Config")
	FLingotionLanguage Language;

	/** When true, all three roles speak at once (as if 4 were pressed) as soon as every preload has succeeded. */
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
