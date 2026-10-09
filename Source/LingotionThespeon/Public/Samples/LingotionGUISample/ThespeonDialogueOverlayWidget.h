// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "Blueprint/UserWidget.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "ThespeonDialogueOverlayWidget.generated.h"

class UAudioStreamComponent;
class AActor;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UThespeonComponent;

/** How a dialogue session ended. */
UENUM(BlueprintType)
enum class EThespeonDialogueResult : uint8
{
	/** Synthesis finished and all audio was played. */
	Completed,
	/** Synthesis reported an error. */
	Failed,
	/** The dialogue was closed before it finished. */
	Cancelled
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThespeonDialogueEnded, EThespeonDialogueResult, Result);

/**
 * Controller for a full-screen dialogue overlay. A Widget Blueprint supplies the
 * layout and styling through the required bound widgets below.
 *
 * On construct the overlay hides itself, spawns a transient actor holding its own Thespeon and audio
 * stream components, and preloads every available character and module on the CPU backend. It then
 * owns synthesis, audio streaming, word-timed text reveal, cancellation and completion for each session.
 */
UCLASS(Abstract, Blueprintable)
class LINGOTIONTHESPEON_API UThespeonDialogueOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

  public:
	/**
	 * Starts a dialogue: cancels any synthesis still running, adds inaudible word markers, shows the
	 * overlay and submits synthesis. Returns false if SessionID is empty or the components can't be created.
	 * Does not fire OnDialogueEnded for a dialogue it replaces.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Dialogue Overlay")
	bool StartDialogue(FLingotionModelInput Input, const FString& SessionID, FInferenceConfig InferenceConfig, UTexture2D* CharacterPortraitTexture);

	/**
	 * Closes the overlay (collapses it; it stays in the widget tree). If playback has already finished it reports
	 * Completed; otherwise it cancels synthesis, clears queued audio and reports Cancelled. Bound to CloseButton.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Dialogue Overlay")
	void CloseDialogue();

	/** Fired once per dialogue when the overlay closes, with the reason it closed. */
	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Dialogue Overlay")
	FOnThespeonDialogueEnded OnDialogueEnded;

  protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Delay after confirmed playback completion before the overlay disappears. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Lingotion Thespeon|Dialogue Overlay", meta = (ClampMin = "0.0"))
	float AutoCloseDelay = 0.5f;

	/** Sample frames subtracted from playback position to compensate for output latency. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Lingotion Thespeon|Dialogue Overlay", meta = (ClampMin = "0"))
	int32 PlaybackLatencyCompensationSamples = 0;

	/** Time each loading-dot state (., .., ...) remains visible before advancing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Lingotion Thespeon|Dialogue Overlay", meta = (ClampMin = "0.01"))
	float LoadingDotInterval = 0.35f;

	// Collapsed when no portrait texture is passed to StartDialogue.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CharacterPortrait;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CharacterNameText;
	// Shows loading dots, then the line revealed in sync with playback.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DialogueText;
	// Calls CloseDialogue.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;

  private:
	UFUNCTION() void HandleAudioReceived(FString ReceivedSessionID, const TArray<float>& SynthesisData);
	UFUNCTION() void HandleAudioSampleRequestReceived(FString ReceivedSessionID, const TArray<int64>& TriggerAudioSamples);
	UFUNCTION() void HandleSynthesisComplete(FString CompletedSessionID);
	UFUNCTION() void HandleSynthesisFailed(FString FailedSessionID);
	UFUNCTION() void HandlePlaybackBufferDrained();

	void PrepareMarkedInput(FLingotionModelInput& Input);
	void RefreshDialogueText();
	void ScheduleAutoClose();
	void AutoCloseDialogue();
	void CloseInternal(EThespeonDialogueResult Result);
	void BindDelegates();
	void UnbindDelegates();
	bool CreateOwnedComponents();
	void DestroyOwnedComponents();
	void PreloadAllModels();
	bool IsCurrentSession(const FString& ReceivedSessionID) const;

	UPROPERTY(Transient) TObjectPtr<UThespeonComponent> ThespeonComponent;
	UPROPERTY(Transient) TObjectPtr<UAudioStreamComponent> AudioStreamComponent;
	UPROPERTY(Transient) TObjectPtr<AActor> ComponentOwnerActor;

	FString ActiveSessionID;
	TArray<FString> DialogueWordChunks;
	TArray<int64> WordMarkerSampleIndices;
	int32 WordMarkerCursor = 0;
	int32 VisibleCharacterCount = 0;
	int32 LoadingDotCount = 1;
	float LoadingDotElapsed = 0.0f;
	bool bSynthesisComplete = false;
	bool bClosing = false;
	bool bPreloadRequested = false;
	bool bWaitingForPlayback = false;
	FTimerHandle AutoCloseTimerHandle;
};
