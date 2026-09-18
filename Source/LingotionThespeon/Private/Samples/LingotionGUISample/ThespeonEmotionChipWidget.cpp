// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Samples/LingotionGUISample/ThespeonEmotionChipWidget.h"

#include "Components/TextBlock.h"

void UThespeonEmotionChipWidget::SetChipData(EEmotion InEmotion, float InWeight)
{
	HiddenCount = 0;
	Emotion = InEmotion;
	Weight = FMath::Clamp(InWeight, 0.0f, 1.0f);
	if (LabelText)
	{
		LabelText->SetText(GetChipLabel());
	}
	OnChipDataSet();
}

void UThespeonEmotionChipWidget::SetOverflowData(int32 InHiddenCount, float InCombinedWeight)
{
	HiddenCount = FMath::Max(0, InHiddenCount);
	Emotion = EEmotion::None;
	Weight = FMath::Clamp(InCombinedWeight, 0.0f, 1.0f);
	if (LabelText)
	{
		LabelText->SetText(GetChipLabel());
	}
	OnChipDataSet();
}

FText UThespeonEmotionChipWidget::GetEmotionName() const
{
	return StaticEnum<EEmotion>()->GetDisplayNameTextByValue(static_cast<int64>(Emotion));
}

FText UThespeonEmotionChipWidget::GetWeightPercentText() const
{
	return FText::FromString(FString::Printf(TEXT("%.0f%%"), Weight * 100.0f));
}

FText UThespeonEmotionChipWidget::GetChipLabel() const
{
	if (HiddenCount > 0)
	{
		return FText::FromString(FString::Printf(TEXT("%d more  %.0f%%"), HiddenCount, Weight * 100.0f));
	}
	if (Emotion == EEmotion::None)
	{
		return GetEmotionName();
	}
	return FText::FromString(FString::Printf(TEXT("%s  %.0f%%"), *GetEmotionName().ToString(), Weight * 100.0f));
}
