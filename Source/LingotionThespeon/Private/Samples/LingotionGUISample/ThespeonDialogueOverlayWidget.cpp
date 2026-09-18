// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Samples/LingotionGUISample/ThespeonDialogueOverlayWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/ManifestHandler.h"
#include "Engine/Texture2D.h"
#include "Engine/ThespeonComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Utils/AudioStreamComponent.h"

void UThespeonDialogueOverlayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseDialogue);
}

void UThespeonDialogueOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	if (CreateOwnedComponents())
	{
		PreloadAllModels();
	}
}

void UThespeonDialogueOverlayWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}
	UnbindDelegates();
	DestroyOwnedComponents();
	Super::NativeDestruct();
}

void UThespeonDialogueOverlayWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!AudioStreamComponent)
	{
		return;
	}

	if (bWaitingForPlayback)
	{
		// Keep the loading indicator until playback reaches the first word. Using
		// the compensated position prevents a blank frame between the dots and text.
		const int64 AudiblePlaybackSample = AudioStreamComponent->GetPlaybackSampleIndex(PlaybackLatencyCompensationSamples);
		const bool bFirstWordReady = !WordMarkerSampleIndices.IsEmpty() && AudiblePlaybackSample >= WordMarkerSampleIndices[0];
		if (!bFirstWordReady)
		{
			LoadingDotElapsed += InDeltaTime;
			const float DotInterval = FMath::Max(LoadingDotInterval, UE_SMALL_NUMBER);
			while (LoadingDotElapsed >= DotInterval)
			{
				LoadingDotElapsed -= DotInterval;
				LoadingDotCount = LoadingDotCount % 3 + 1;
				DialogueText->SetText(FText::FromString(FString::ChrN(LoadingDotCount, TEXT('.'))));
			}
			return;
		}

		bWaitingForPlayback = false;
		RefreshDialogueText();
	}

	if (DialogueWordChunks.IsEmpty() || WordMarkerSampleIndices.IsEmpty())
	{
		return;
	}

	const int64 AudibleSample = AudioStreamComponent->GetPlaybackSampleIndex(PlaybackLatencyCompensationSamples);
	const int32 PreviousMarkerCursor = WordMarkerCursor;
	const int32 PreviousVisibleCharacterCount = VisibleCharacterCount;
	const int32 ExpectedMarkerCount = DialogueWordChunks.Num() + 1;
	const int32 AvailableMarkerCount = FMath::Min(WordMarkerSampleIndices.Num(), ExpectedMarkerCount);
	while (WordMarkerCursor < AvailableMarkerCount && AudibleSample >= WordMarkerSampleIndices[WordMarkerCursor])
	{
		++WordMarkerCursor;
	}

	VisibleCharacterCount = 0;
	if (WordMarkerCursor > 0 && WordMarkerCursor <= DialogueWordChunks.Num() && WordMarkerCursor < AvailableMarkerCount)
	{
		const int32 WordIndex = WordMarkerCursor - 1;
		const FString& WordChunk = DialogueWordChunks[WordIndex];
		int32 LeadingWhitespaceCount = 0;
		while (LeadingWhitespaceCount < WordChunk.Len() && FChar::IsWhitespace(WordChunk[LeadingWhitespaceCount]))
		{
			++LeadingWhitespaceCount;
		}

		const int32 WordCharacterCount = WordChunk.Len() - LeadingWhitespaceCount;
		const int64 WordStartSample = WordMarkerSampleIndices[WordIndex];
		const int64 WordEndSample = WordMarkerSampleIndices[WordIndex + 1];
		if (WordCharacterCount > 0)
		{
			if (WordEndSample <= WordStartSample)
			{
				VisibleCharacterCount = WordCharacterCount;
			}
			else
			{
				const int64 ElapsedWordSamples = FMath::Clamp(AudibleSample - WordStartSample, int64{0}, WordEndSample - WordStartSample);
				const double CharacterProgress =
				    static_cast<double>(ElapsedWordSamples) * WordCharacterCount / static_cast<double>(WordEndSample - WordStartSample);
				VisibleCharacterCount = FMath::Min(FMath::FloorToInt(CharacterProgress) + 1, WordCharacterCount);
			}
		}
	}

	if (WordMarkerCursor != PreviousMarkerCursor || VisibleCharacterCount != PreviousVisibleCharacterCount)
	{
		RefreshDialogueText();
	}
}

bool UThespeonDialogueOverlayWidget::StartDialogue(
    FLingotionModelInput Input, const FString& SessionID, const FInferenceConfig InferenceConfig, UTexture2D* CharacterPortraitTexture
)
{
	if (SessionID.IsEmpty())
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}
	UnbindDelegates();

	if ((!ThespeonComponent || !AudioStreamComponent) && !CreateOwnedComponents())
	{
		return false;
	}
	PreloadAllModels();
	ActiveSessionID = SessionID;
	WordMarkerSampleIndices.Reset();
	WordMarkerCursor = 0;
	VisibleCharacterCount = 0;
	LoadingDotCount = 1;
	LoadingDotElapsed = 0.0f;
	bSynthesisComplete = false;
	bClosing = false;
	bWaitingForPlayback = true;

	const FString CharacterName = Input.CharacterName;
	PrepareMarkedInput(Input);
	CharacterNameText->SetText(FText::FromString(CharacterName));
	CharacterPortrait->SetBrushFromTexture(CharacterPortraitTexture, true);
	CharacterPortrait->SetVisibility(CharacterPortraitTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DialogueText->SetText(FText::FromString(TEXT(".")));
	BindDelegates();
	AudioStreamComponent->ResetBuffer();
	AudioStreamComponent->Start();

	SetVisibility(ESlateVisibility::Visible);
	ThespeonComponent->Synthesize(MoveTemp(Input), SessionID, InferenceConfig);
	return true;
}

void UThespeonDialogueOverlayWidget::CloseDialogue()
{
	CloseInternal(true);
}

void UThespeonDialogueOverlayWidget::HandleAudioReceived(const FString ReceivedSessionID, const TArray<float>& SynthesisData)
{
	if (IsCurrentSession(ReceivedSessionID) && AudioStreamComponent)
	{
		AudioStreamComponent->SubmitAudioToStream(SynthesisData);
	}
}

void UThespeonDialogueOverlayWidget::HandleAudioSampleRequestReceived(const FString ReceivedSessionID, const TArray<int64>& TriggerAudioSamples)
{
	if (!IsCurrentSession(ReceivedSessionID))
	{
		return;
	}
	WordMarkerSampleIndices = TriggerAudioSamples;
	WordMarkerCursor = 0;
	VisibleCharacterCount = 0;
}

void UThespeonDialogueOverlayWidget::HandleSynthesisComplete(const FString CompletedSessionID)
{
	if (IsCurrentSession(CompletedSessionID))
	{
		bSynthesisComplete = true;
	}
}

void UThespeonDialogueOverlayWidget::HandleSynthesisFailed(const FString FailedSessionID)
{
	if (IsCurrentSession(FailedSessionID))
	{
		CloseInternal(false);
	}
}

void UThespeonDialogueOverlayWidget::HandlePlaybackBufferDrained()
{
	// A drain can be transient while synthesis is still producing packets. Only a
	// drain after synthesis completion confirms that all generated audio was played.
	if (!bSynthesisComplete)
	{
		return;
	}
	WordMarkerCursor = DialogueWordChunks.Num() + 1;
	VisibleCharacterCount = 0;
	bWaitingForPlayback = false;
	RefreshDialogueText();
	ScheduleAutoClose();
}

void UThespeonDialogueOverlayWidget::PrepareMarkedInput(FLingotionModelInput& Input)
{
	DialogueWordChunks.Reset();
	FString PendingWhitespace;
	const TCHAR Marker = Thespeon::ControlCharacters::AudioSampleRequest;
	const FString MarkerString(1, &Marker);

	int32 LastSpokenSegmentIndex = INDEX_NONE;
	for (int32 SegmentIndex = 0; SegmentIndex < Input.Segments.Num(); ++SegmentIndex)
	{
		FLingotionInputSegment& Segment = Input.Segments[SegmentIndex];
		FString MarkedText;
		MarkedText.Reserve(Segment.Text.Len() + Segment.Text.Len() / 4);
		int32 TextIndex = 0;
		while (TextIndex < Segment.Text.Len())
		{
			if (Segment.Text[TextIndex] == Marker)
			{
				// Rebuild markers deterministically if this input was prepared before.
				++TextIndex;
				continue;
			}
			if (FChar::IsWhitespace(Segment.Text[TextIndex]))
			{
				PendingWhitespace.AppendChar(Segment.Text[TextIndex]);
				MarkedText.AppendChar(Segment.Text[TextIndex]);
				++TextIndex;
				continue;
			}

			const int32 WordStart = TextIndex;
			while (TextIndex < Segment.Text.Len() && !FChar::IsWhitespace(Segment.Text[TextIndex]))
			{
				++TextIndex;
			}
			FString Word = Segment.Text.Mid(WordStart, TextIndex - WordStart);
			Word.ReplaceInline(*MarkerString, TEXT(""));
			if (Word.IsEmpty())
			{
				continue;
			}
			DialogueWordChunks.Add(PendingWhitespace + Word);
			LastSpokenSegmentIndex = SegmentIndex;
			PendingWhitespace.Reset();
			MarkedText.AppendChar(Marker);
			MarkedText += Word;
		}
		Segment.Text = MoveTemp(MarkedText);
	}

	// Every displayed word needs a following timestamp to define its reveal
	// interval. This final marker supplies the end timestamp for the last word.
	if (LastSpokenSegmentIndex != INDEX_NONE)
	{
		Input.Segments[LastSpokenSegmentIndex].Text.AppendChar(Marker);
	}
}

void UThespeonDialogueOverlayWidget::RefreshDialogueText()
{
	FString RevealedText;
	const int32 CompleteWordCount = FMath::Min(FMath::Max(WordMarkerCursor - 1, 0), DialogueWordChunks.Num());
	for (int32 Index = 0; Index < CompleteWordCount; ++Index)
	{
		RevealedText += DialogueWordChunks[Index];
	}

	if (WordMarkerCursor > 0 && WordMarkerCursor <= DialogueWordChunks.Num())
	{
		const FString& CurrentWordChunk = DialogueWordChunks[WordMarkerCursor - 1];
		int32 LeadingWhitespaceCount = 0;
		while (LeadingWhitespaceCount < CurrentWordChunk.Len() && FChar::IsWhitespace(CurrentWordChunk[LeadingWhitespaceCount]))
		{
			++LeadingWhitespaceCount;
		}
		RevealedText += CurrentWordChunk.Left(LeadingWhitespaceCount + VisibleCharacterCount);
	}
	DialogueText->SetText(FText::FromString(RevealedText));
}

void UThespeonDialogueOverlayWidget::ScheduleAutoClose()
{
	if (bClosing)
	{
		return;
	}
	if (AutoCloseDelay <= 0.0f)
	{
		AutoCloseDialogue();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AutoCloseTimerHandle, this, &ThisClass::AutoCloseDialogue, AutoCloseDelay, false);
	}
}

void UThespeonDialogueOverlayWidget::AutoCloseDialogue()
{
	CloseInternal(false);
}

void UThespeonDialogueOverlayWidget::CloseInternal(const bool bInterruptPlayback)
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	bWaitingForPlayback = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}
	UnbindDelegates();
	if (bInterruptPlayback)
	{
		if (ThespeonComponent)
		{
			ThespeonComponent->CancelSynthesis();
		}
		if (AudioStreamComponent)
		{
			AudioStreamComponent->ResetBuffer();
		}
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UThespeonDialogueOverlayWidget::BindDelegates()
{
	ThespeonComponent->OnAudioReceived.AddUniqueDynamic(this, &ThisClass::HandleAudioReceived);
	ThespeonComponent->OnAudioSampleRequestReceived.AddUniqueDynamic(this, &ThisClass::HandleAudioSampleRequestReceived);
	ThespeonComponent->OnSynthesisComplete.AddUniqueDynamic(this, &ThisClass::HandleSynthesisComplete);
	ThespeonComponent->OnSynthesisFailed.AddUniqueDynamic(this, &ThisClass::HandleSynthesisFailed);
	AudioStreamComponent->OnPlaybackBufferDrained.AddUniqueDynamic(this, &ThisClass::HandlePlaybackBufferDrained);
}

void UThespeonDialogueOverlayWidget::UnbindDelegates()
{
	if (ThespeonComponent)
	{
		ThespeonComponent->OnAudioReceived.RemoveDynamic(this, &ThisClass::HandleAudioReceived);
		ThespeonComponent->OnAudioSampleRequestReceived.RemoveDynamic(this, &ThisClass::HandleAudioSampleRequestReceived);
		ThespeonComponent->OnSynthesisComplete.RemoveDynamic(this, &ThisClass::HandleSynthesisComplete);
		ThespeonComponent->OnSynthesisFailed.RemoveDynamic(this, &ThisClass::HandleSynthesisFailed);
	}
	if (AudioStreamComponent)
	{
		AudioStreamComponent->OnPlaybackBufferDrained.RemoveDynamic(this, &ThisClass::HandlePlaybackBufferDrained);
	}
}

bool UThespeonDialogueOverlayWidget::CreateOwnedComponents()
{
	if (IsValid(ComponentOwnerActor) && IsValid(ThespeonComponent) && IsValid(AudioStreamComponent))
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (!World || IsDesignTime())
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ComponentOwnerActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!ComponentOwnerActor)
	{
		return false;
	}

	ThespeonComponent = NewObject<UThespeonComponent>(ComponentOwnerActor);
	AudioStreamComponent = NewObject<UAudioStreamComponent>(ComponentOwnerActor);
	if (!ThespeonComponent || !AudioStreamComponent)
	{
		DestroyOwnedComponents();
		return false;
	}

	ComponentOwnerActor->AddInstanceComponent(ThespeonComponent);
	ComponentOwnerActor->AddInstanceComponent(AudioStreamComponent);
	ThespeonComponent->RegisterComponent();
	AudioStreamComponent->RegisterComponent();
	if (!ThespeonComponent->IsRegistered() || !AudioStreamComponent->IsRegistered())
	{
		DestroyOwnedComponents();
		return false;
	}
	return true;
}

void UThespeonDialogueOverlayWidget::DestroyOwnedComponents()
{
	if (ThespeonComponent)
	{
		if (ThespeonComponent->IsSynthesizing())
		{
			ThespeonComponent->CancelSynthesis();
		}
		ThespeonComponent = nullptr;
	}
	if (AudioStreamComponent)
	{
		AudioStreamComponent->ResetBuffer();
		AudioStreamComponent->Stop();
		AudioStreamComponent = nullptr;
	}
	if (ComponentOwnerActor)
	{
		ComponentOwnerActor->Destroy();
		ComponentOwnerActor = nullptr;
	}
}

void UThespeonDialogueOverlayWidget::PreloadAllModels()
{
	if (bPreloadRequested || !ThespeonComponent)
	{
		return;
	}
	const UManifestHandler* Manifest = UManifestHandler::Get();
	if (!Manifest)
	{
		return;
	}

	FInferenceConfig CPUConfig;
	CPUConfig.BackendType = EBackendType::CPU;
	TArray<FPreloadEntry> Entries;
	for (const FString& CharacterName : Manifest->GetAllAvailableCharacters())
	{
		for (const TPair<EThespeonModuleType, FString>& Module : Manifest->GetModuleTypesOfCharacter(CharacterName))
		{
			FPreloadEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.CharacterName = CharacterName;
			Entry.ModuleType = Module.Key;
			Entry.InferenceConfig = CPUConfig;
		}
	}
	if (Entries.IsEmpty())
	{
		return;
	}

	bPreloadRequested = true;
	ThespeonComponent->PreloadCharacterGroup(MoveTemp(Entries), TEXT("LingotionGUISample_AllModels_CPU"));
}

bool UThespeonDialogueOverlayWidget::IsCurrentSession(const FString& ReceivedSessionID) const
{
	return !bClosing && ReceivedSessionID == ActiveSessionID;
}
