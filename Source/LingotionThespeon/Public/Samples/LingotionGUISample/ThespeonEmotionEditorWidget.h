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

/** Modal editor for an emotion TMap. Duplicate rows are combined and weights are normalized on submit. */
UCLASS(Abstract, Blueprintable)
class LINGOTIONTHESPEON_API UThespeonEmotionEditorWidget : public UUserWidget
{
	GENERATED_BODY()

  public:
	void Open(UAdvancedThespeonWidget* InOwner, bool bInForStart, const TMap<EEmotion, float>& InitialMap);
	void RemoveRow(UThespeonEmotionRowWidget* Row);

	/** Called by a row when its emotion or weight changes, so the live status preview can update. */
	void NotifyRowChanged();

  protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	TSubclassOf<UThespeonEmotionRowWidget> EmotionRowClass;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> EmotionRowsBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> AddEmotionButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SubmitEmotionButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CancelEmotionButton;
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
