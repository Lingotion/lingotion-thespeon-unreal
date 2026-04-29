// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "AAngelDevilDemoActor.h"
#include "Engine/ThespeonComponent.h"
#include "Core/LingotionLogger.h"
#include "Core/ModelInput.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

// Role labels and HUD colors
const FString AAngelDevilDemoActor::RoleLabels[NumRoles] = {TEXT("PERSON"), TEXT("ANGEL"), TEXT("DEVIL")};
const FColor AAngelDevilDemoActor::RoleColors[NumRoles] = {FColor::White, FColor::Cyan, FColor::Red};

AAngelDevilDemoActor::AAngelDevilDemoActor()
{
	PrimaryActorTick.bCanEverTick = true;

	const TCHAR* CompNames[NumRoles] = {TEXT("PersonComp"), TEXT("AngelComp"), TEXT("DevilComp")};
	const TCHAR* AudioNames[NumRoles] = {TEXT("PersonAudio"), TEXT("AngelAudio"), TEXT("DevilAudio")};

	Roles.SetNum(NumRoles);
	Roles[0] = {TEXT("Aaron Archer"), EEmotion::Interest, TEXT("I found a wallet on the ground. What should I do?")};
	Roles[1] = {
	    TEXT("Aaron Archer"),
	    EEmotion::Serenity,
	    TEXT("You should return it to the owner. It is the right thing to do, and you will feel good about yourself.")
	};
	Roles[2] = {
	    TEXT("Aaron Archer"), EEmotion::Anger, TEXT("Keep the money! Nobody will ever know. Finders keepers, that is the rule of the jungle.")
	};

	Comps.SetNum(NumRoles);
	AudioComps.SetNum(NumRoles);
	for (int32 i = 0; i < NumRoles; ++i)
	{
		Comps[i] = CreateDefaultSubobject<UThespeonComponent>(CompNames[i]);
		AudioComps[i] = CreateDefaultSubobject<UAudioStreamComponent>(AudioNames[i]);
	}
}

void AAngelDevilDemoActor::BeginPlay()
{
	Super::BeginPlay();

	if (Language.ISO639_2.IsEmpty())
	{
		Language.ISO639_2 = TEXT("eng");
	}

	for (int32 i = 0; i < NumRoles; ++i)
	{
		AudioComps[i]->Start();
		Roles[i].PreloadStatus = EDemoStatus::InProgress;
	}

	// Bind preload callbacks (per-index trampolines: no session ID, CharacterName not unique across roles)
	Comps[0]->OnPreloadComplete.AddDynamic(this, &AAngelDevilDemoActor::OnPreloadComplete0);
	Comps[1]->OnPreloadComplete.AddDynamic(this, &AAngelDevilDemoActor::OnPreloadComplete1);
	Comps[2]->OnPreloadComplete.AddDynamic(this, &AAngelDevilDemoActor::OnPreloadComplete2);

	// Bind audio & synthesis callbacks (shared: index resolved at runtime via SessionToIndex)
	for (int32 i = 0; i < NumRoles; ++i)
	{
		Comps[i]->OnAudioReceived.AddDynamic(this, &AAngelDevilDemoActor::OnAudioReceived);
		Comps[i]->OnSynthesisComplete.AddDynamic(this, &AAngelDevilDemoActor::OnSynthComplete);
		Comps[i]->OnSynthesisFailed.AddDynamic(this, &AAngelDevilDemoActor::OnSynthFailed);
	}

	EnableInput(GetWorld()->GetFirstPlayerController());

	PreloadStartTime = FPlatformTime::Seconds();

	LINGO_LOG(EVerbosityLevel::Info, TEXT("=== ANGEL & DEVIL DEMO: Preloading 3 characters ==="));
	for (int32 i = 0; i < NumRoles; ++i)
	{
		LINGO_LOG(
		    EVerbosityLevel::Info, TEXT("  [%s] %s (Emotion: %s)"), *RoleLabels[i], *Roles[i].Character, *UEnum::GetValueAsString(Roles[i].Emotion)
		);
		Comps[i]->PreloadCharacter(Roles[i].Character, ModuleType);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Cyan, TEXT("Angel & Devil Demo: Loading characters..."));
		GEngine->AddOnScreenDebugMessage(-1, 60.0f, FColor::White, TEXT("Press 1=Person | 2=Angel | 3=Devil | 4=ALL speak | 5=Cancel"));
	}
}

const FString AAngelDevilDemoActor::GetPreloadString(EDemoStatus Status, double Elapsed, double TotalElapsed)
{
	switch (Status)
	{
		case EDemoStatus::Idle:
			return TEXT("idle");
		case EDemoStatus::InProgress:
			return FString::Printf(TEXT("loading... (%.1fs)"), Elapsed);
		case EDemoStatus::Success:
			return FString::Printf(TEXT("ready (%.2fs)"), TotalElapsed);
		case EDemoStatus::Failed:
			return FString::Printf(TEXT("FAILED (%.2fs)"), TotalElapsed);
		default:
			return TEXT("unknown");
	}
}

const FString AAngelDevilDemoActor::GetSynthString(EDemoStatus Status, double Elapsed, double TotalElapsed, int32 Packets)
{
	switch (Status)
	{
		case EDemoStatus::Idle:
			return TEXT("idle");
		case EDemoStatus::InProgress:
			return FString::Printf(TEXT("speaking... (%.1fs, %d pkts)"), Elapsed, Packets);
		case EDemoStatus::Success:
			return FString::Printf(TEXT("DONE (%.2fs, %d pkts)"), TotalElapsed, Packets);
		case EDemoStatus::Failed:
			return FString::Printf(TEXT("FAILED (%.2fs)"), TotalElapsed);
		default:
			return TEXT("unknown");
	}
}

void AAngelDevilDemoActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::One))
	{
		SynthesizeRole(0);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::Two))
	{
		SynthesizeRole(1);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::Three))
	{
		SynthesizeRole(2);
	}
	else if (PC->WasInputKeyJustPressed(EKeys::Four))
	{
		SynthesizeAll();
	}
	else if (PC->WasInputKeyJustPressed(EKeys::Five))
	{
		LINGO_LOG(EVerbosityLevel::Info, TEXT("=== CANCELLING ALL ==="));
		for (int32 i = 0; i < NumRoles; ++i)
		{
			Comps[i]->CancelSynthesis();
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, TEXT("Cancelled all."));
		}
	}

	// On-screen HUD
	if (GEngine)
	{
		const double Now = FPlatformTime::Seconds();

		for (int32 i = 0; i < NumRoles; ++i)
		{
			const FRoleEntry& RoleEntry = Roles[i];

			FString PreloadStr = GetPreloadString(RoleEntry.PreloadStatus, Now - PreloadStartTime, RoleEntry.PreloadElapsed);
			FString SynthStr = GetSynthString(RoleEntry.SynthStatus, Now - SynthStartTime, RoleEntry.SynthElapsed, RoleEntry.PacketsReceived);

			GEngine->AddOnScreenDebugMessage(
			    100 + i, 0.0f, RoleColors[i], FString::Printf(TEXT("[%s] %s  Preload: %s"), *RoleLabels[i], *RoleEntry.Character, *PreloadStr)
			);
			GEngine->AddOnScreenDebugMessage(200 + i, 0.0f, RoleColors[i], FString::Printf(TEXT("    Synth: %s"), *SynthStr));
		}
	}
}

// ---------------------------------------------------------------------------
// Preload callbacks
// ---------------------------------------------------------------------------

void AAngelDevilDemoActor::OnPreloadComplete0(bool bSuccess, FString CharacterName, EThespeonModuleType CompModuleType, EBackendType BackendType)
{
	HandlePreloadComplete(0, bSuccess, CharacterName);
}
void AAngelDevilDemoActor::OnPreloadComplete1(bool bSuccess, FString CharacterName, EThespeonModuleType CompModuleType, EBackendType BackendType)
{
	HandlePreloadComplete(1, bSuccess, CharacterName);
}
void AAngelDevilDemoActor::OnPreloadComplete2(bool bSuccess, FString CharacterName, EThespeonModuleType CompModuleType, EBackendType BackendType)
{
	HandlePreloadComplete(2, bSuccess, CharacterName);
}

void AAngelDevilDemoActor::HandlePreloadComplete(int32 Index, bool bSuccess, const FString& CharacterName)
{
	const double Elapsed = FPlatformTime::Seconds() - PreloadStartTime;
	Roles[Index].PreloadElapsed = Elapsed;
	Roles[Index].PreloadStatus = bSuccess ? EDemoStatus::Success : EDemoStatus::Failed;

	LINGO_LOG(
	    EVerbosityLevel::Info,
	    TEXT("=== [%s] %s preload: %s at %.2fs ==="),
	    *RoleLabels[Index],
	    *CharacterName,
	    bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
	    Elapsed
	);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		    -1,
		    10.0f,
		    bSuccess ? FColor::Green : FColor::Red,
		    FString::Printf(TEXT("[%s] %s preload %s (%.2fs)"), *RoleLabels[Index], *CharacterName, bSuccess ? TEXT("OK") : TEXT("FAILED"), Elapsed)
		);
	}

	if (AllPreloadsComplete())
	{
		LINGO_LOG(EVerbosityLevel::Info, TEXT("=== ALL CHARACTERS READY — Press 4 to start the conversation! ==="));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Green, TEXT("All ready! Press 4 for the Angel & Devil conversation!"));
		}
		if (bAutoSynthesizeOnPreloadComplete)
		{
			SynthesizeAll();
		}
	}
}

// ---------------------------------------------------------------------------
// Audio & synthesis callbacks
// ---------------------------------------------------------------------------

void AAngelDevilDemoActor::OnAudioReceived(FString SessionID, const TArray<float>& SynthData)
{
	const int32* IndexPtr = SessionToIndex.Find(SessionID);
	if (!IndexPtr)
	{
		return;
	}
	const int32 Index = *IndexPtr;
	Roles[Index].PacketsReceived++;
	if (AudioComps.IsValidIndex(Index) && AudioComps[Index])
	{
		AudioComps[Index]->SubmitAudioToStream(SynthData);
	}
}

void AAngelDevilDemoActor::OnSynthComplete(FString SessionID)
{
	HandleSynthComplete(SessionToIndex.FindRef(SessionID), SessionID, true);
}
void AAngelDevilDemoActor::OnSynthFailed(FString SessionID)
{
	HandleSynthComplete(SessionToIndex.FindRef(SessionID), SessionID, false);
}

void AAngelDevilDemoActor::HandleSynthComplete(int32 Index, const FString& SessionID, bool bSuccess)
{
	SessionToIndex.Remove(SessionID);

	const double Elapsed = FPlatformTime::Seconds() - SynthStartTime;
	FRoleEntry& RoleEntry = Roles[Index];
	RoleEntry.SynthElapsed = Elapsed;
	RoleEntry.SynthStatus = bSuccess ? EDemoStatus::Success : EDemoStatus::Failed;

	LINGO_LOG(
	    EVerbosityLevel::Info,
	    TEXT("=== [%s] %s: %s at %.2fs (%d pkts) ==="),
	    *RoleLabels[Index],
	    *RoleEntry.Character,
	    bSuccess ? TEXT("DONE") : TEXT("FAILED"),
	    Elapsed,
	    RoleEntry.PacketsReceived
	);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		    -1,
		    10.0f,
		    bSuccess ? FColor::Green : FColor::Red,
		    FString::Printf(
		        TEXT("[%s] %s (%.2fs, %d pkts)"), *RoleLabels[Index], bSuccess ? TEXT("done") : TEXT("FAILED"), Elapsed, RoleEntry.PacketsReceived
		    )
		);
	}

	if (bConcurrentSynthActive && AllSynthsComplete())
	{
		bConcurrentSynthActive = false;

		double MaxTime = 0.0, SumTime = 0.0;
		int32 TotalPackets = 0;
		bool bAllOK = true;
		for (const FRoleEntry& R : Roles)
		{
			MaxTime = FMath::Max(MaxTime, R.SynthElapsed);
			SumTime += R.SynthElapsed;
			TotalPackets += R.PacketsReceived;
			bAllOK = bAllOK && (R.SynthStatus == EDemoStatus::Success);
		}
		const double Ratio = (SumTime > 0.0) ? MaxTime / SumTime : 1.0;

		LINGO_LOG(EVerbosityLevel::Info, TEXT(""));
		LINGO_LOG(EVerbosityLevel::Info, TEXT("========================================================"));
		LINGO_LOG(EVerbosityLevel::Info, TEXT("  ANGEL & DEVIL RESULTS"));
		LINGO_LOG(EVerbosityLevel::Info, TEXT("========================================================"));
		for (int32 i = 0; i < NumRoles; ++i)
		{
			LINGO_LOG(EVerbosityLevel::Info, TEXT("  [%s] %.2fs (%d pkts)"), *RoleLabels[i], Roles[i].SynthElapsed, Roles[i].PacketsReceived);
		}
		LINGO_LOG(EVerbosityLevel::Info, TEXT("  max=%.2fs  sum=%.2fs  ratio=%.2f  total=%d pkts"), MaxTime, SumTime, Ratio, TotalPackets);

		if (Ratio < 0.7 && bAllOK)
		{
			LINGO_LOG(EVerbosityLevel::Info, TEXT("  VERDICT: PARALLEL (ratio %.2f < 0.7) — all 3 spoke simultaneously!"), Ratio);
		}
		else if (!bAllOK)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("  VERDICT: FAILURE — one or more voices failed"));
		}
		else
		{
			LINGO_LOG(EVerbosityLevel::Warning, TEXT("  VERDICT: Possibly sequential (ratio %.2f >= 0.7)"), Ratio);
		}
		LINGO_LOG(EVerbosityLevel::Info, TEXT("========================================================"));

		if (GEngine)
		{
			const FString Verdict = (Ratio < 0.7 && bAllOK) ? TEXT("PARALLEL") : (!bAllOK ? TEXT("FAILED") : TEXT("SEQUENTIAL?"));
			const FColor VColor = (Ratio < 0.7 && bAllOK) ? FColor::Green : (!bAllOK ? FColor::Red : FColor::Yellow);
			GEngine->AddOnScreenDebugMessage(
			    -1, 30.0f, VColor, FString::Printf(TEXT("Conversation done! ratio=%.2f => %s (%d pkts)"), Ratio, *Verdict, TotalPackets)
			);
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void AAngelDevilDemoActor::SynthesizeRole(int32 Index)
{
	if (!Roles.IsValidIndex(Index) || !Comps.IsValidIndex(Index) || !Comps[Index])
	{
		return;
	}

	FRoleEntry& RoleEntry = Roles[Index];

	if (RoleEntry.PreloadStatus != EDemoStatus::Success)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("[%s] %s not ready — preload first!"), *RoleLabels[Index], *RoleEntry.Character);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("[%s] not preloaded yet!"), *RoleLabels[Index]));
		}
		return;
	}

	FLingotionModelInput Input;
	Input.CharacterName = RoleEntry.Character;
	Input.ModuleType = ModuleType;
	Input.DefaultEmotion = RoleEntry.Emotion;
	Input.DefaultLanguage = Language;

	FLingotionInputSegment Segment;
	Segment.Text = RoleEntry.Text;
	Segment.Language = Language;
	Segment.Emotion = RoleEntry.Emotion;
	Input.Segments.Add(Segment);

	const FString SessionId = FString::Printf(TEXT("AngelDevil_%s_%lld"), *RoleLabels[Index], FPlatformTime::Cycles64());
	SessionToIndex.Add(SessionId, Index);

	RoleEntry.SynthStatus = EDemoStatus::InProgress;
	RoleEntry.PacketsReceived = 0;
	if (SynthStartTime == 0.0 || !bConcurrentSynthActive)
	{
		SynthStartTime = FPlatformTime::Seconds();
	}

	LINGO_LOG(
	    EVerbosityLevel::Info,
	    TEXT("=== [%s] %s speaking (emotion: %s) ==="),
	    *RoleLabels[Index],
	    *RoleEntry.Character,
	    *UEnum::GetValueAsString(RoleEntry.Emotion)
	);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, RoleColors[Index], FString::Printf(TEXT("[%s] Speaking..."), *RoleLabels[Index]));
	}

	Comps[Index]->Synthesize(Input, SessionId);
}

void AAngelDevilDemoActor::SynthesizeAll()
{
	if (!AllPreloadsComplete())
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Cannot start conversation — not all characters preloaded"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Not all characters preloaded!"));
		}
		return;
	}

	LINGO_LOG(EVerbosityLevel::Info, TEXT(""));
	LINGO_LOG(EVerbosityLevel::Info, TEXT("=== ANGEL & DEVIL: Starting conversation — 3 voices simultaneously ==="));
	for (int32 i = 0; i < NumRoles; ++i)
	{
		LINGO_LOG(
		    EVerbosityLevel::Info,
		    TEXT("  [%s] %s: \"%s\" (Emotion: %s)"),
		    *RoleLabels[i],
		    *Roles[i].Character,
		    *Roles[i].Text,
		    *UEnum::GetValueAsString(Roles[i].Emotion)
		);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, TEXT("Angel & Devil conversation starting — 3 voices at once!"));
	}

	bConcurrentSynthActive = true;
	SynthStartTime = FPlatformTime::Seconds();
	for (FRoleEntry& RoleEntry : Roles)
	{
		RoleEntry.SynthStatus = EDemoStatus::Idle;
		RoleEntry.SynthElapsed = 0.0;
		RoleEntry.PacketsReceived = 0;
	}

	for (int32 i = 0; i < NumRoles; ++i)
	{
		SynthesizeRole(i);
	}
}

bool AAngelDevilDemoActor::AllPreloadsComplete() const
{
	for (const FRoleEntry& RoleEntry : Roles)
	{
		if (RoleEntry.PreloadStatus != EDemoStatus::Success)
			return false;
	}
	return true;
}

bool AAngelDevilDemoActor::AllSynthsComplete() const
{
	for (const FRoleEntry& RoleEntry : Roles)
	{
		if (RoleEntry.SynthStatus == EDemoStatus::Idle || RoleEntry.SynthStatus == EDemoStatus::InProgress)
			return false;
	}
	return true;
}
