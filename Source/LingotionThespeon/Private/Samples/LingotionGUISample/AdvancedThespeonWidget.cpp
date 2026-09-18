// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Samples/LingotionGUISample/AdvancedThespeonWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Core/LingotionLogger.h"
#include "Core/ManifestHandler.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "JsonObjectConverter.h"
#include "Samples/LingotionGUISample/ThespeonDialogueOverlayWidget.h"
#include "Samples/LingotionGUISample/ThespeonEmotionChipWidget.h"
#include "Samples/LingotionGUISample/ThespeonEmotionEditorWidget.h"

void UAdvancedThespeonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CharacterComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleCharacterChanged);
	ModuleTypeComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleModuleTypeChanged);
	BackendComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleBackendChanged);
	LanguageComboBox->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleLanguageChanged);
	InputTextBox->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleTextChanged);
	StartSpeedSpinBox->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleStartSpeedChanged);
	StartLoudnessSpinBox->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleStartLoudnessChanged);
	EndSpeedSpinBox->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleEndSpeedChanged);
	EndLoudnessSpinBox->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleEndLoudnessChanged);
	StartEmotionEditButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenStartEmotionEditor);
	EndEmotionEditButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenEndEmotionEditor);
	SynthesizeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Synthesize);
	if (PreviousSegmentButton)
	{
		PreviousSegmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SelectPreviousSegment);
	}
	if (NextSegmentButton)
	{
		NextSegmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SelectNextSegment);
	}
	if (CreateSegmentButton)
	{
		CreateSegmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CreateSegment);
	}
	if (DeleteSegmentButton)
	{
		DeleteSegmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::DeleteSegment);
	}
}

void UAdvancedThespeonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (GlobalControls)
	{
		GlobalControls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	// Seed the segment's speed/loudness ramp from the designer-configured spin boxes so their initial values are respected
	FLingotionInputSegment& Segment = GetSelectedSegment();
	if (StartSpeedSpinBox)
	{
		Segment.StartSpeed = StartSpeedSpinBox->GetValue();
	}
	if (EndSpeedSpinBox)
	{
		Segment.EndSpeed = EndSpeedSpinBox->GetValue();
	}
	if (StartLoudnessSpinBox)
	{
		Segment.StartLoudness = StartLoudnessSpinBox->GetValue();
	}
	if (EndLoudnessSpinBox)
	{
		Segment.EndLoudness = EndLoudnessSpinBox->GetValue();
	}
	PopulateBackends();
	PopulateCharacters();
	RefreshAll();
	RefreshSegmentControls();
	LogSegment(TEXT("GUI initialization"));
	InputTextBox->SetKeyboardFocus();
}

FReply UAdvancedThespeonWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::D)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Ctrl+D pressed: toggling developer mode."));
		ToggleDeveloperMode();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FLingotionModelInput UAdvancedThespeonWidget::BuildModelInput() const
{
	FLingotionModelInput Input;
	Input.CharacterName = CharacterName;
	Input.ModuleType = ModuleType;
	if (Segments.IsEmpty())
	{
		return Input;
	}
	Input.Segments = Segments;
	// The first segment supplies the input-wide defaults for language and emotion.
	const FLingotionInputSegment& FirstSegment = Segments[0];
	Input.DefaultLanguage = FirstSegment.Language;
	float HighestWeight = -1.0f;
	for (const TPair<EEmotion, float>& Pair : FirstSegment.StartEmotion)
	{
		if (Pair.Key != EEmotion::None && Pair.Value > HighestWeight)
		{
			Input.DefaultEmotion = Pair.Key;
			HighestWeight = Pair.Value;
		}
	}
	return Input;
}

FInferenceConfig UAdvancedThespeonWidget::BuildInferenceConfig() const
{
	FInferenceConfig Config;
	Config.BackendType = BackendType;
	Config.ModuleType = ModuleType;
	const FLingotionModelInput Input = BuildModelInput();
	Config.FallbackLanguage = Input.DefaultLanguage;
	if (Input.DefaultEmotion != EEmotion::None)
	{
		Config.FallbackEmotion = Input.DefaultEmotion;
	}
	return Config;
}

void UAdvancedThespeonWidget::PopulateCharacters()
{
	bRefreshingOptions = true;
	CharacterComboBox->ClearOptions();
	TArray<FString> Characters;
	if (const UManifestHandler* Manifest = UManifestHandler::Get())
	{
		Characters = Manifest->GetAllAvailableCharacters().Array();
	}
	Characters.Sort();
	for (const FString& Character : Characters)
	{
		CharacterComboBox->AddOption(Character);
	}
	if (!Characters.Contains(CharacterName))
	{
		CharacterName = Characters.IsEmpty() ? FString() : Characters[0];
	}
	if (!CharacterName.IsEmpty())
	{
		CharacterComboBox->SetSelectedOption(CharacterName);
	}
	bRefreshingOptions = false;
	PopulateModuleTypes();
}

void UAdvancedThespeonWidget::PopulateModuleTypes()
{
	bRefreshingOptions = true;
	ModuleTypeComboBox->ClearOptions();
	ModuleTypeOptions.Reset();
	ModuleIds.Reset();
	if (const UManifestHandler* Manifest = UManifestHandler::Get(); Manifest && !CharacterName.IsEmpty())
	{
		ModuleIds = Manifest->GetModuleTypesOfCharacter(CharacterName);
	}
	const EThespeonModuleType OrderedTypes[] = {
	    EThespeonModuleType::XL, EThespeonModuleType::L, EThespeonModuleType::M, EThespeonModuleType::S, EThespeonModuleType::XS
	};
	for (const EThespeonModuleType Type : OrderedTypes)
	{
		if (ModuleIds.Contains(Type))
		{
			const FString Label = StaticEnum<EThespeonModuleType>()->GetDisplayNameTextByValue(static_cast<int64>(Type)).ToString();
			ModuleTypeOptions.Add(Label, Type);
			ModuleTypeComboBox->AddOption(Label);
		}
	}
	if (!ModuleIds.Contains(ModuleType))
	{
		if (ModuleIds.Contains(EThespeonModuleType::XL))
		{
			ModuleType = EThespeonModuleType::XL;
		}
		else
		{
			ModuleType = EThespeonModuleType::None;
			for (const EThespeonModuleType Type : OrderedTypes)
			{
				if (ModuleIds.Contains(Type))
				{
					ModuleType = Type;
					break;
				}
			}
		}
	}
	for (const TPair<FString, EThespeonModuleType>& Option : ModuleTypeOptions)
	{
		if (Option.Value == ModuleType)
		{
			ModuleTypeComboBox->SetSelectedOption(Option.Key);
			break;
		}
	}
	bRefreshingOptions = false;
	RefreshLanguage();
}

void UAdvancedThespeonWidget::PopulateBackends()
{
	bRefreshingOptions = true;
	BackendComboBox->ClearOptions();
	BackendOptions = {{TEXT("Default"), EBackendType::None}, {TEXT("CPU"), EBackendType::CPU}, {TEXT("GPU"), EBackendType::GPU}};
	for (const FString Label : {FString(TEXT("Default")), FString(TEXT("CPU")), FString(TEXT("GPU"))})
	{
		BackendComboBox->AddOption(Label);
		if (BackendOptions[Label] == BackendType)
		{
			BackendComboBox->SetSelectedOption(Label);
		}
	}
	bRefreshingOptions = false;
}

void UAdvancedThespeonWidget::HandleCharacterChanged(FString SelectedItem, ESelectInfo::Type)
{
	if (bRefreshingOptions || SelectedItem == CharacterName)
	{
		return;
	}
	CharacterName = MoveTemp(SelectedItem);
	PopulateModuleTypes();
	RefreshPortrait();
	OnStateChanged.Broadcast();
}

void UAdvancedThespeonWidget::HandleModuleTypeChanged(FString SelectedItem, ESelectInfo::Type)
{
	if (bRefreshingOptions)
	{
		return;
	}
	if (const EThespeonModuleType* SelectedType = ModuleTypeOptions.Find(SelectedItem))
	{
		ModuleType = *SelectedType;
		RefreshLanguage();
		OnStateChanged.Broadcast();
	}
}

void UAdvancedThespeonWidget::HandleBackendChanged(FString SelectedItem, ESelectInfo::Type)
{
	if (!bRefreshingOptions)
	{
		if (const EBackendType* SelectedBackend = BackendOptions.Find(SelectedItem))
		{
			BackendType = *SelectedBackend;
			OnStateChanged.Broadcast();
		}
	}
}

void UAdvancedThespeonWidget::HandleLanguageChanged(FString SelectedItem, ESelectInfo::Type)
{
	if (bRefreshingOptions)
	{
		return;
	}
	if (const int32* Index = LanguageOptions.Find(SelectedItem); Index && AvailableLanguages.IsValidIndex(*Index))
	{
		GetSelectedSegment().Language = AvailableLanguages[*Index];
		OnStateChanged.Broadcast();
	}
}

void UAdvancedThespeonWidget::HandleTextChanged(const FText& Text)
{
	if (bRefreshingControls)
	{
		return;
	}
	FString NewText = Text.ToString();
	if (MaxInputTextLength > 0 && NewText.Len() > MaxInputTextLength)
	{
		NewText.LeftInline(MaxInputTextLength);
		// SetText re-enters this handler, so suppress the recursion with the existing refresh guard.
		bRefreshingControls = true;
		InputTextBox->SetText(FText::FromString(NewText));
		bRefreshingControls = false;
	}
	GetSelectedSegment().Text = NewText;
	LogSegment(TEXT("text edit"));
	OnStateChanged.Broadcast();
}

void UAdvancedThespeonWidget::HandleStartSpeedChanged(float Value)
{
	if (!bRefreshingControls)
	{
		GetSelectedSegment().StartSpeed = Value;
		OnStateChanged.Broadcast();
	}
}

void UAdvancedThespeonWidget::HandleStartLoudnessChanged(float Value)
{
	if (!bRefreshingControls)
	{
		GetSelectedSegment().StartLoudness = Value;
		OnStateChanged.Broadcast();
	}
}

void UAdvancedThespeonWidget::HandleEndSpeedChanged(float Value)
{
	if (!bRefreshingControls)
	{
		GetSelectedSegment().EndSpeed = Value;
		OnStateChanged.Broadcast();
	}
}

void UAdvancedThespeonWidget::HandleEndLoudnessChanged(float Value)
{
	if (!bRefreshingControls)
	{
		GetSelectedSegment().EndLoudness = Value;
		OnStateChanged.Broadcast();
	}
}

void UAdvancedThespeonWidget::OpenStartEmotionEditor()
{
	OpenEmotionEditor(true);
}

void UAdvancedThespeonWidget::OpenEndEmotionEditor()
{
	OpenEmotionEditor(false);
}

void UAdvancedThespeonWidget::OpenEmotionEditor(bool bForStart)
{
	TSubclassOf<UThespeonEmotionEditorWidget> DialogClass = EmotionEditorClass;
	if (!DialogClass)
	{
		DialogClass = LoadClass<UThespeonEmotionEditorWidget>(
		    nullptr, TEXT("/LingotionThespeon/Samples/LingotionGUISample/Widgets/W_ThespeonEmotionEditorModal.W_ThespeonEmotionEditorModal_C")
		);
	}
	if (!DialogClass)
	{
		UE_LOG(
		    LogTemp,
		    Error,
		    TEXT(
		        "OpenEmotionEditor failed: EmotionEditorClass is unset and W_ThespeonEmotionEditorModal could not be loaded. Set Emotion Editor Class in WP_ThespeonGUI Class Defaults."
		    )
		);
		return;
	}
	UThespeonEmotionEditorWidget* Dialog = GetOwningPlayer() ? CreateWidget<UThespeonEmotionEditorWidget>(GetOwningPlayer(), DialogClass)
	                                                         : CreateWidget<UThespeonEmotionEditorWidget>(GetWorld(), DialogClass);
	if (Dialog)
	{
		const FLingotionInputSegment& Segment = GetSelectedSegment();
		Dialog->Open(this, bForStart, bForStart ? Segment.StartEmotion : Segment.EndEmotion);
		Dialog->SetIsEnabled(true);
		Dialog->SetRenderOpacity(1.0f);
		Dialog->SetVisibility(ESlateVisibility::Visible);
		Dialog->AddToViewport(110);
		Dialog->SetFocus();
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("OpenEmotionEditor failed: CreateWidget returned null for class '%s'."), *GetNameSafe(DialogClass.Get()));
}

void UAdvancedThespeonWidget::ApplyEmotionMap(bool bForStart, const TMap<EEmotion, float>& EmotionMap)
{
	FLingotionInputSegment& Segment = GetSelectedSegment();
	(bForStart ? Segment.StartEmotion : Segment.EndEmotion) = EmotionMap;
	RefreshEmotionSummaries();
	LogSegment(TEXT("emotion edit"));
	OnStateChanged.Broadcast();
}

void UAdvancedThespeonWidget::RefreshAll()
{
	RefreshPortrait();
	RefreshControls();
	RefreshEmotionSummaries();
}

void UAdvancedThespeonWidget::RefreshControls()
{
	bRefreshingControls = true;
	const FLingotionInputSegment& Segment = GetSelectedSegment();
	if (InputTextBox)
	{
		InputTextBox->SetText(FText::FromString(Segment.Text));
	}
	if (StartSpeedSpinBox)
	{
		StartSpeedSpinBox->SetValue(Segment.StartSpeed);
	}
	if (StartLoudnessSpinBox)
	{
		StartLoudnessSpinBox->SetValue(Segment.StartLoudness);
	}
	if (EndSpeedSpinBox)
	{
		EndSpeedSpinBox->SetValue(Segment.EndSpeed);
	}
	if (EndLoudnessSpinBox)
	{
		EndLoudnessSpinBox->SetValue(Segment.EndLoudness);
	}
	bRefreshingControls = false;
}

void UAdvancedThespeonWidget::RefreshEmotionSummaries()
{
	const FLingotionInputSegment& Segment = GetSelectedSegment();
	BuildEmotionChips(StartEmotionSummary, Segment.StartEmotion);
	BuildEmotionChips(EndEmotionSummary, Segment.EndEmotion);
}

void UAdvancedThespeonWidget::RefreshLanguage()
{
	bRefreshingOptions = true;
	AvailableLanguages.Reset();
	LanguageOptions.Reset();
	LanguageComboBox->ClearOptions();
	if (const UManifestHandler* Manifest = UManifestHandler::Get())
	{
		const FString ModuleId = GetSelectedModuleId();
		if (!ModuleId.IsEmpty())
		{
			AvailableLanguages = Manifest->GetAllLanguagesInCharacterModule(ModuleId);
		}
	}
	for (int32 Index = 0; Index < AvailableLanguages.Num(); ++Index)
	{
		FString Label = LanguageLabel(AvailableLanguages[Index]);
		if (LanguageOptions.Contains(Label))
		{
			Label += FString::Printf(TEXT(" (%d)"), Index + 1);
		}
		LanguageOptions.Add(Label, Index);
		LanguageComboBox->AddOption(Label);
	}
	FLingotionInputSegment& Segment = GetSelectedSegment();
	int32 SelectedLanguageIndex = AvailableLanguages.IndexOfByKey(Segment.Language);
	if (SelectedLanguageIndex == INDEX_NONE && !AvailableLanguages.IsEmpty())
	{
		SelectedLanguageIndex = 0;
		Segment.Language = AvailableLanguages[0];
	}
	for (const TPair<FString, int32>& Option : LanguageOptions)
	{
		if (Option.Value == SelectedLanguageIndex)
		{
			LanguageComboBox->SetSelectedOption(Option.Key);
			break;
		}
	}
	LanguageComboBox->SetIsEnabled(!AvailableLanguages.IsEmpty());
	bRefreshingOptions = false;
}

UTexture2D* UAdvancedThespeonWidget::ResolvePortraitTexture() const
{
	const TSoftObjectPtr<UTexture2D>* Portrait = CharacterPortraits.Find(CharacterName);
	if (!Portrait)
	{
		const FString NormalizedCharacterName = CharacterName.TrimStartAndEnd();
		for (const TPair<FString, TSoftObjectPtr<UTexture2D>>& Pair : CharacterPortraits)
		{
			if (Pair.Key.TrimStartAndEnd().Equals(NormalizedCharacterName, ESearchCase::IgnoreCase))
			{
				Portrait = &Pair.Value;
				break;
			}
		}
	}
	const TSoftObjectPtr<UTexture2D>* PortraitToLoad =
	    Portrait ? Portrait : (!DefaultCharacterPortrait.IsNull() ? &DefaultCharacterPortrait : nullptr);
	return PortraitToLoad ? PortraitToLoad->LoadSynchronous() : nullptr;
}

void UAdvancedThespeonWidget::RefreshPortrait()
{
	if (!CharacterPortrait)
	{
		return;
	}
	if (UTexture2D* Texture = ResolvePortraitTexture())
	{
		CharacterPortrait->SetBrushFromTexture(Texture, false);
		CharacterPortrait->SetColorAndOpacity(FLinearColor::White);
		CharacterPortrait->SetOpacity(1.0f);
		CharacterPortrait->SetVisibility(ESlateVisibility::Visible);
		CharacterPortrait->InvalidateLayoutAndVolatility();
		return;
	}
	TArray<FString> ConfiguredNames;
	CharacterPortraits.GetKeys(ConfiguredNames);
	UE_LOG(
	    LogTemp, Warning, TEXT("No portrait resolved for '%s'. Configured keys: [%s]"), *CharacterName, *FString::Join(ConfiguredNames, TEXT(", "))
	);
	CharacterPortrait->SetVisibility(ESlateVisibility::Hidden);
}

void UAdvancedThespeonWidget::Synthesize()
{
	if (CharacterName.IsEmpty() || ModuleType == EThespeonModuleType::None)
	{
		SetStatus(FText::FromString(TEXT("Select a character and module.")), true);
		return;
	}
	if (GetSelectedSegment().Text.TrimStartAndEnd().IsEmpty())
	{
		SetStatus(FText::FromString(TEXT("Enter some text to synthesize.")), true);
		return;
	}
	if (!DialogueOverlay)
	{
		SetStatus(FText::FromString(TEXT("DialogueOverlay is not bound on W_ThespeonGUI.")), true);
		return;
	}
	const FString SessionId = FString::Printf(TEXT("%s_%s"), *SessionIdPrefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	if (DialogueOverlay->StartDialogue(BuildModelInput(), SessionId, BuildInferenceConfig(), ResolvePortraitTexture()))
	{
		SetStatus(FText::FromString(TEXT("Synthesis submitted.")));
	}
	else
	{
		SetStatus(FText::FromString(TEXT("Failed to start dialogue (could not create synthesis components).")), true);
	}
}

FLingotionInputSegment& UAdvancedThespeonWidget::GetSelectedSegment()
{
	if (Segments.IsEmpty())
	{
		Segments.Add(FLingotionInputSegment());
	}
	SelectedSegmentIndex = FMath::Clamp(SelectedSegmentIndex, 0, Segments.Num() - 1);
	return Segments[SelectedSegmentIndex];
}

const FLingotionInputSegment& UAdvancedThespeonWidget::GetSelectedSegment() const
{
	if (Segments.IsEmpty())
	{
		static const FLingotionInputSegment EmptySegment;
		return EmptySegment;
	}
	return Segments[FMath::Clamp(SelectedSegmentIndex, 0, Segments.Num() - 1)];
}

void UAdvancedThespeonWidget::SelectPreviousSegment()
{
	if (SelectedSegmentIndex <= 0)
	{
		return;
	}
	--SelectedSegmentIndex;
	RefreshControls();
	RefreshEmotionSummaries();
	RefreshLanguage();
	RefreshSegmentControls();
	LogSegment(TEXT("select previous segment"));
}

void UAdvancedThespeonWidget::SelectNextSegment()
{
	if (SelectedSegmentIndex >= Segments.Num() - 1)
	{
		return;
	}
	++SelectedSegmentIndex;
	RefreshControls();
	RefreshEmotionSummaries();
	RefreshLanguage();
	RefreshSegmentControls();
	LogSegment(TEXT("select next segment"));
}

void UAdvancedThespeonWidget::CreateSegment()
{
	GetSelectedSegment(); // Guarantee a valid array and index before inserting.
	const int32 InsertAt = SelectedSegmentIndex + 1;
	Segments.Insert(FLingotionInputSegment(), InsertAt);
	SelectedSegmentIndex = InsertAt;
	RefreshControls();
	RefreshEmotionSummaries();
	RefreshLanguage();
	RefreshSegmentControls();
	LogSegment(TEXT("create segment"));
	OnStateChanged.Broadcast();
}

void UAdvancedThespeonWidget::DeleteSegment()
{
	if (!Segments.IsValidIndex(SelectedSegmentIndex))
	{
		return;
	}
	// Don't lose the removed segment's text: fold it into a neighbour if one exists.
	const FString RemovedText = Segments[SelectedSegmentIndex].Text;
	const bool bHasPredecessor = SelectedSegmentIndex > 0;
	const bool bHasSuccessor = SelectedSegmentIndex < Segments.Num() - 1;
	if (bHasPredecessor)
	{
		// Append to the preceding segment.
		Segments[SelectedSegmentIndex - 1].Text += RemovedText;
	}
	else if (bHasSuccessor)
	{
		// No predecessor: prepend to the following segment instead.
		Segments[SelectedSegmentIndex + 1].Text = RemovedText + Segments[SelectedSegmentIndex + 1].Text;
	}
	Segments.RemoveAt(SelectedSegmentIndex);
	if (bHasPredecessor)
	{
		// Follow the merged text back to the predecessor.
		--SelectedSegmentIndex;
	}
	// Never leave the line without a segment: recreate an empty one if this was the last.
	if (Segments.IsEmpty())
	{
		Segments.Add(FLingotionInputSegment());
	}
	SelectedSegmentIndex = FMath::Clamp(SelectedSegmentIndex, 0, Segments.Num() - 1);
	RefreshControls();
	RefreshEmotionSummaries();
	RefreshLanguage();
	RefreshSegmentControls();
	LogSegment(TEXT("delete segment"));
	OnStateChanged.Broadcast();
}

void UAdvancedThespeonWidget::ToggleDeveloperMode()
{
	bDeveloperMode = !bDeveloperMode;
	if (!bDeveloperMode)
	{
		// Leaving developer mode: fold every segment's text into the first segment and drop the rest.
		if (Segments.Num() > 1)
		{
			FString CombinedText;
			for (const FLingotionInputSegment& Seg : Segments)
			{
				CombinedText += Seg.Text;
			}
			Segments[0].Text = CombinedText;
			Segments.RemoveAt(1, Segments.Num() - 1);
		}
		SelectedSegmentIndex = 0;
		RefreshControls();
		RefreshEmotionSummaries();
		RefreshLanguage();
		LogSegment(TEXT("developer mode collapse"));
		OnStateChanged.Broadcast();
	}
	RefreshSegmentControls();
}

void UAdvancedThespeonWidget::RefreshSegmentControls()
{
	// Toggle the whole container so the buttons and any labels inside it show/hide together.
	if (SegmentControl)
	{
		SegmentControl->SetVisibility(bDeveloperMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	// The arrows still enable/disable individually based on where we are in the array.
	if (PreviousSegmentButton)
	{
		PreviousSegmentButton->SetIsEnabled(SelectedSegmentIndex > 0);
	}
	if (NextSegmentButton)
	{
		NextSegmentButton->SetIsEnabled(SelectedSegmentIndex < Segments.Num() - 1);
	}
}

void UAdvancedThespeonWidget::LogSegment(const TCHAR* EditContext) const
{
	FString SegmentJson;
	if (!FJsonObjectConverter::UStructToJsonObjectString(GetSelectedSegment(), SegmentJson))
	{
		SegmentJson = TEXT("{\"serializationError\":true}");
	}
	LINGO_LOG(
	    EVerbosityLevel::Debug, TEXT("Advanced GUI segment %d/%d after %s:\n%s"), SelectedSegmentIndex + 1, Segments.Num(), EditContext, *SegmentJson
	);
}

FString UAdvancedThespeonWidget::GetSelectedModuleId() const
{
	if (const FString* ModuleId = ModuleIds.Find(ModuleType))
	{
		return *ModuleId;
	}
	return FString();
}

FString UAdvancedThespeonWidget::LanguageLabel(const FLingotionLanguage& Language)
{
	return Language.ISO3166_1.IsEmpty() ? Language.ISO639_2 : Language.ISO639_2 + TEXT(" (") + Language.ISO3166_1 + TEXT(")");
}

void UAdvancedThespeonWidget::SetStatus(const FText& Message, bool bIsError)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(bIsError ? FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f)) : FSlateColor(FLinearColor::White));
	}
}

void UAdvancedThespeonWidget::BuildEmotionChips(UWrapBox* Container, const TMap<EEmotion, float>& EmotionMap)
{
	if (!Container)
	{
		return;
	}
	Container->ClearChildren();
	if (!EmotionChipClass)
	{
		UE_LOG(
		    LogTemp,
		    Warning,
		    TEXT("Emotion summary not shown: EmotionChipClass is unset. Assign it in W_ThespeonGUI Class Defaults to spawn emotion chips.")
		);
		return;
	}

	// Only positive, non-None weights are meaningful; show the dominant emotion first.
	TArray<TPair<EEmotion, float>> Entries;
	for (const TPair<EEmotion, float>& Pair : EmotionMap)
	{
		if (Pair.Key != EEmotion::None && Pair.Value > 0.0f)
		{
			Entries.Add(Pair);
		}
	}
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

	if (Entries.IsEmpty())
	{
		AddEmotionChip(Container, EEmotion::None, 1.0f);
		return;
	}

	// Cap the visible chips so a heavy blend can't grow the box too much; the rest collapse
	// into a single "N more" chip. Only collapse when it actually saves space (>1 hidden).
	const int32 Cap = FMath::Max(1, MaxSummaryChips);
	if (Entries.Num() <= Cap + 1)
	{
		for (const TPair<EEmotion, float>& Entry : Entries)
		{
			AddEmotionChip(Container, Entry.Key, Entry.Value);
		}
		return;
	}
	float CombinedHiddenWeight = 0.0f;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Index < Cap)
		{
			AddEmotionChip(Container, Entries[Index].Key, Entries[Index].Value);
		}
		else
		{
			CombinedHiddenWeight += Entries[Index].Value;
		}
	}
	AddOverflowChip(Container, Entries.Num() - Cap, CombinedHiddenWeight);
}

void UAdvancedThespeonWidget::AddEmotionChip(UWrapBox* Container, EEmotion Emotion, float Weight)
{
	if (!Container || !WidgetTree || !EmotionChipClass)
	{
		return;
	}
	UThespeonEmotionChipWidget* Chip = WidgetTree->ConstructWidget<UThespeonEmotionChipWidget>(EmotionChipClass);
	if (!Chip)
	{
		return;
	}
	Chip->SetChipData(Emotion, Weight);
	if (UWrapBoxSlot* ChipSlot = Container->AddChildToWrapBox(Chip))
	{
		// Gap between chips as they reflow across rows.
		ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 6.0f));
	}
}

void UAdvancedThespeonWidget::AddOverflowChip(UWrapBox* Container, int32 HiddenCount, float CombinedWeight)
{
	if (!Container || !WidgetTree || !EmotionChipClass)
	{
		return;
	}
	UThespeonEmotionChipWidget* Chip = WidgetTree->ConstructWidget<UThespeonEmotionChipWidget>(EmotionChipClass);
	if (!Chip)
	{
		return;
	}
	Chip->SetOverflowData(HiddenCount, CombinedWeight);
	if (UWrapBoxSlot* ChipSlot = Container->AddChildToWrapBox(Chip))
	{
		// Match the gap used between emotion chips.
		ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 6.0f));
	}
}
