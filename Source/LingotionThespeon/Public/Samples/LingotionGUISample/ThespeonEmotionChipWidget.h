// Copyright 2025 - 2026 Lingotion AB All Rights Reserved
#pragma once

#include "Blueprint/UserWidget.h"
#include "Core/ModelInput.h"
#include "ThespeonEmotionChipWidget.generated.h"

class UTextBlock;

/**
 * A single emotion "chip" shown in the Advanced GUI start/end emotion summary.
 *
 * Create a Blueprint subclass to design the chip graphically. The summary spawns one instance per
 * emotion in the blend and calls SetChipData(). To drive the visuals, either:
 *   - name a TextBlock "LabelText" and the base fills it with "<Emotion>  <NN>%" automatically, or
 *   - override the OnChipDataSet event and bind GetEmotionName() / GetWeightPercentText() / GetWeight()
 *     to your own layout (colored background, icon, mini bar, etc.).
 */
UCLASS(Abstract, Blueprintable)
class LINGOTIONTHESPEON_API UThespeonEmotionChipWidget : public UUserWidget
{
	GENERATED_BODY()

  public:
	/** Sets the emotion and its normalized weight (0-1), refreshes the optional bound label, and fires OnChipDataSet. */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Advanced GUI")
	void SetChipData(EEmotion InEmotion, float InWeight);

	/**
	 * Turns this chip into the "N more  X%" overflow indicator that stands in for the emotions the
	 * summary hid. HiddenCount is how many were collapsed, CombinedWeight their summed weight (0-1).
	 */
	UFUNCTION(BlueprintCallable, Category = "Lingotion Thespeon|Advanced GUI")
	void SetOverflowData(int32 InHiddenCount, float InCombinedWeight);

	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Advanced GUI")
	EEmotion GetEmotion() const
	{
		return Emotion;
	}

	/** Normalized weight in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Advanced GUI")
	float GetWeight() const
	{
		return Weight;
	}

	/** Localized display name of the emotion (e.g. "Joy"). */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Advanced GUI")
	FText GetEmotionName() const;

	/** Weight formatted as a whole percentage (e.g. "55%"). */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Advanced GUI")
	FText GetWeightPercentText() const;

	/** Combined label (e.g. "Joy  55%"), or the overflow label (e.g. "3 more  20%") when this is an overflow chip. */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Advanced GUI")
	FText GetChipLabel() const;

  protected:
	/** Fired after SetChipData stores the values. Override in Blueprint to drive custom visuals. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lingotion Thespeon|Advanced GUI")
	void OnChipDataSet();

	/** Optional: name a TextBlock "LabelText" and the base auto-fills it with GetChipLabel(). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	EEmotion Emotion = EEmotion::None;

	UPROPERTY(BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	float Weight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	int32 HiddenCount = 0;
};
