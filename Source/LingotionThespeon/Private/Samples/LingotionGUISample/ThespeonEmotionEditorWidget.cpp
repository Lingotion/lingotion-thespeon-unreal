// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Samples/LingotionGUISample/ThespeonEmotionEditorWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Core/KeypointUtils.h"
#include "Samples/LingotionGUISample/AdvancedThespeonWidget.h"

namespace
{
bool IsHiddenEnumEntry(const UEnum* Enum, int32 Index)
{
	return Enum->GetNameStringByIndex(Index).EndsWith(TEXT("_MAX"));
}
} // namespace

void UThespeonEmotionRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EmotionComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleEmotionChanged);
	IntensitySlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleWeightChanged);
	RemoveButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemove);
	PopulateEmotions();
}

void UThespeonEmotionRowWidget::PopulateEmotions()
{
	bRefreshing = true;
	EmotionOptions.Reset();
	EmotionComboBox->ClearOptions();
	const UEnum* EmotionEnum = StaticEnum<EEmotion>();
	for (int32 Index = 0; Index < EmotionEnum->NumEnums(); ++Index)
	{
		const int64 Value = EmotionEnum->GetValueByIndex(Index);
		if (Value == INDEX_NONE || IsHiddenEnumEntry(EmotionEnum, Index))
		{
			continue;
		}
		const FString Label = EmotionEnum->GetDisplayNameTextByIndex(Index).ToString();
		EmotionOptions.Add(Label, static_cast<EEmotion>(Value));
		EmotionComboBox->AddOption(Label);
	}
	bRefreshing = false;
}

void UThespeonEmotionRowWidget::InitializeRow(UThespeonEmotionEditorWidget* InOwner, EEmotion InEmotion, float InWeight)
{
	bRefreshing = true;
	OwnerDialog = InOwner;
	Emotion = InEmotion;
	Weight = FMath::Clamp(InWeight, 0.0f, 1.0f);
	IntensitySlider->SetMinValue(0.0f);
	IntensitySlider->SetMaxValue(1.0f);
	IntensitySlider->SetValue(Weight);
	for (const TPair<FString, EEmotion>& Option : EmotionOptions)
	{
		if (Option.Value == Emotion)
		{
			EmotionComboBox->SetSelectedOption(Option.Key);
			break;
		}
	}
	RefreshWeightText();
	bRefreshing = false;
}

void UThespeonEmotionRowWidget::HandleEmotionChanged(FString SelectedItem, ESelectInfo::Type)
{
	if (!bRefreshing)
	{
		if (const EEmotion* SelectedEmotion = EmotionOptions.Find(SelectedItem))
		{
			Emotion = *SelectedEmotion;
		}
		if (OwnerDialog)
		{
			OwnerDialog->NotifyRowChanged();
		}
	}
}

void UThespeonEmotionRowWidget::HandleWeightChanged(float Value)
{
	if (!bRefreshing)
	{
		Weight = FMath::Clamp(Value, 0.0f, 1.0f);
		RefreshWeightText();
		if (OwnerDialog)
		{
			OwnerDialog->NotifyRowChanged();
		}
	}
}

void UThespeonEmotionRowWidget::HandleRemove()
{
	if (OwnerDialog)
	{
		OwnerDialog->RemoveRow(this);
	}
}

void UThespeonEmotionRowWidget::RefreshWeightText()
{
	IntensityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Weight * 100.0f)));
}

void UThespeonEmotionEditorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	AddEmotionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::AddEmotionRow);
	SubmitEmotionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Submit);
	CancelEmotionButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Cancel);
}

void UThespeonEmotionEditorWidget::Open(UAdvancedThespeonWidget* InOwner, bool bInForStart, const TMap<EEmotion, float>& InitialMap)
{
	Owner = InOwner;
	bForStart = bInForStart;
	Rows.Reset();
	EmotionRowsBox->ClearChildren();
	TArray<EEmotion> Emotions;
	InitialMap.GetKeys(Emotions);
	Emotions.Sort([](EEmotion Left, EEmotion Right) { return static_cast<uint8>(Left) < static_cast<uint8>(Right); });
	for (EEmotion Emotion : Emotions)
	{
		AddEmotionRow(Emotion, InitialMap.FindRef(Emotion));
	}
	if (Rows.IsEmpty())
	{
		AddEmotionRow(EEmotion::None, 1.0f);
	}
	RefreshStatus();
}

void UThespeonEmotionEditorWidget::AddEmotionRow()
{
	EEmotion Candidate = EEmotion::None;
	const UEnum* EmotionEnum = StaticEnum<EEmotion>();
	for (int32 Index = 0; Index < EmotionEnum->NumEnums(); ++Index)
	{
		const int64 Value = EmotionEnum->GetValueByIndex(Index);
		if (Value == INDEX_NONE || IsHiddenEnumEntry(EmotionEnum, Index))
		{
			continue;
		}
		const EEmotion Possible = static_cast<EEmotion>(Value);
		if (Possible == EEmotion::None)
		{
			continue;
		}
		if (!Rows.ContainsByPredicate([Possible](const UThespeonEmotionRowWidget* Row) { return Row && Row->GetEmotion() == Possible; }))
		{
			Candidate = Possible;
			break;
		}
	}

	AddEmotionRow(Candidate, 1.0f);
}

void UThespeonEmotionEditorWidget::AddEmotionRow(EEmotion Emotion, float Weight)
{
	if (!EmotionRowClass)
	{
		if (EmotionStatusText)
		{
			EmotionStatusText->SetText(FText::FromString(TEXT("Assign EmotionRowClass on this widget.")));
		}
		return;
	}
	if (UThespeonEmotionRowWidget* Row = CreateWidget<UThespeonEmotionRowWidget>(GetWorld(), EmotionRowClass))
	{
		Rows.Add(Row);
		EmotionRowsBox->AddChild(Row);
		Row->InitializeRow(this, Emotion, Weight);
	}
	RefreshStatus();
}

void UThespeonEmotionEditorWidget::RemoveRow(UThespeonEmotionRowWidget* Row)
{
	if (Row)
	{
		EmotionRowsBox->RemoveChild(Row);
		Rows.Remove(Row);
	}
	if (Rows.IsEmpty())
	{
		AddEmotionRow(EEmotion::None, 1.0f);
	}
	RefreshStatus();
}

TMap<EEmotion, float> UThespeonEmotionEditorWidget::ComputeNormalizedBlend() const
{
	TMap<EEmotion, float> Blend;
	for (const UThespeonEmotionRowWidget* Row : Rows)
	{
		if (Row && Row->GetWeight() > 0.0f)
		{
			Blend.FindOrAdd(Row->GetEmotion()) += Row->GetWeight();
		}
	}
	if (!Thespeon::Core::SanitizeEmotionBlend(Blend))
	{
		// Nothing with positive weight: represent it as the neutral "no emotion" state.
		Blend.Add(EEmotion::None, 1.0f);
	}
	return Blend;
}

void UThespeonEmotionEditorWidget::RefreshStatus()
{
	if (!EmotionStatusText || !EmotionRowClass)
	{
		return;
	}
	TArray<TPair<EEmotion, float>> Entries = ComputeNormalizedBlend().Array();
	Entries.Sort(
	    [](const TPair<EEmotion, float>& A, const TPair<EEmotion, float>& B)
	    {
		    if (!FMath::IsNearlyEqual(A.Value, B.Value))
		    {
			    return A.Value > B.Value;
		    }
		    return static_cast<uint8>(A.Key) < static_cast<uint8>(B.Key);
	    }
	);
	const UEnum* EmotionEnum = StaticEnum<EEmotion>();
	TArray<FString> Parts;
	Parts.Reserve(Entries.Num());
	for (const TPair<EEmotion, float>& Entry : Entries)
	{
		const FString Name = EmotionEnum->GetDisplayNameTextByValue(static_cast<int64>(Entry.Key)).ToString();
		Parts.Add(FString::Printf(TEXT("%s %.0f%%"), *Name, Entry.Value * 100.0f));
	}
	EmotionStatusText->SetText(FText::FromString(FString::Join(Parts, TEXT(", "))));
}

void UThespeonEmotionEditorWidget::NotifyRowChanged()
{
	RefreshStatus();
}

void UThespeonEmotionEditorWidget::Submit()
{
	const TMap<EEmotion, float> Result = ComputeNormalizedBlend();
	if (Owner)
	{
		Owner->ApplyEmotionMap(bForStart, Result);
	}
	RemoveFromParent();
}

void UThespeonEmotionEditorWidget::Cancel()
{
	RemoveFromParent();
}
