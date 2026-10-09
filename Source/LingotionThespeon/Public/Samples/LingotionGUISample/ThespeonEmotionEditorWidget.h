// Copyright 2025 - 2026 Lingotion AB All Rights Reserved
#pragma once

#include "Blueprint/UserWidget.h"
#include "Core/ModelInput.h"
#include "ThespeonEmotionEditorWidget.generated.h"

class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;
class UScrollBox;
class UThespeonEmotionEditorWidget;
class UAdvancedThespeonWidget;

/** One editable emotion/weight entry in the emotion modal. */
UCLASS(Abstract, Blueprintable)
class LINGOTIONTHESPEON_API UThespeonEmotionRowWidget : public UUserWidget
{
	GENERATED_BODY()

  public:
	void InitializeRow(UThespeonEmotionEditorWidget* InOwner, EEmotion InEmotion, float InWeight);
	EEmotion GetEmotion() const
	{
		return Emotion;
	}
	float GetWeight() const
	{
		return Weight;
	}

  protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> EmotionComboBox;
	// Range is forced to 0-1 in code.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> IntensitySlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> IntensityValueText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RemoveButton;

  private:
	UFUNCTION() void HandleEmotionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleWeightChanged(float Value);
	UFUNCTION() void HandleRemove();
	void PopulateEmotions();
	void RefreshWeightText();

	UPROPERTY(Transient) TObjectPtr<UThespeonEmotionEditorWidget> OwnerDialog;
	TMap<FString, EEmotion> EmotionOptions;
	EEmotion Emotion = EEmotion::None;
	float Weight = 1.0f;
	bool bRefreshing = false;
};

/**
 * Modal editor for an emotion blend (TMap<EEmotion, float>). Duplicate rows are summed, zero-weight rows are dropped
 * and weights are normalized; an empty blend becomes None at 100%. Submit applies the result to the owner's selected
 * segment and Cancel discards it. Both close the modal.
 */
UCLASS(Abstract, Blueprintable)
class LINGOTIONTHESPEON_API UThespeonEmotionEditorWidget : public UUserWidget
{
	GENERATED_BODY()

  public:
	/** Fills the modal with one row per emotion in InitialMap, sorted by enum value; adds a None row if the map is empty. */
	void Open(UAdvancedThespeonWidget* InOwner, bool bInForStart, const TMap<EEmotion, float>& InitialMap);
	/** Removes a row. Removing the last row adds a None row in its place. */
	void RemoveRow(UThespeonEmotionRowWidget* Row);

	/** Called by a row when its emotion or weight changes, so the live status preview can update. */
	void NotifyRowChanged();

  protected:
	virtual void NativeOnInitialized() override;

	/** Row widget class. Required: if unset, no rows are created and EmotionStatusText asks you to assign it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	TSubclassOf<UThespeonEmotionRowWidget> EmotionRowClass;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> EmotionRowsBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> AddEmotionButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SubmitEmotionButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CancelEmotionButton;
	// Optional live preview of the normalized blend.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmotionStatusText;

  private:
	UFUNCTION() void AddEmotionRow();
	UFUNCTION() void Submit();
	UFUNCTION() void Cancel();
	void AddEmotionRow(EEmotion Emotion, float Weight);

	/** Gathers the rows into the exact normalized blend Submit would apply (falls back to None when empty). */
	TMap<EEmotion, float> ComputeNormalizedBlend() const;
	/** Recomputes the blend and writes a human-readable preview to EmotionStatusText. */
	void RefreshStatus();

	UPROPERTY(Transient) TObjectPtr<UAdvancedThespeonWidget> Owner;
	UPROPERTY(Transient) TArray<TObjectPtr<UThespeonEmotionRowWidget>> Rows;
	bool bForStart = false;
};
