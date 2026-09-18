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

/**
 * Controller for a full-screen dialogue overlay. A Widget Blueprint supplies the
 * layout and styling through the required bound widgets below.
 *
 * The overlay resolves the required actor components and owns synthesis, audio
 * streaming, word timing, cancellation, and completion for its session.
 */
UCLASS(Abstract, Blueprintable)
class LINGOTIONTHESPEON_API UThespeonDialogueOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

  public:
	/**
	 * Resolves components, adds inaudible word markers, shows the overlay, and
	 * submits synthesis.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Dialogue Overlay")
	bool StartDialogue(FLingotionModelInput Input, const FString& SessionID, FInferenceConfig InferenceConfig, UTexture2D* CharacterPortraitTexture);

	/** Cancels synthesis, clears queued playback, and removes the overlay. */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Dialogue Overlay")
	void CloseDialogue();

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

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CharacterPortrait;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CharacterNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DialogueText;
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
	void CloseInternal(bool bInterruptPlayback);
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
