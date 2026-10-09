// Copyright 2025 - 2026 Lingotion AB All Rights Reserved
#pragma once

#include "Blueprint/UserWidget.h"
#include "Core/BackendType.h"
#include "Core/ModelInput.h"
#include "Engine/InferenceConfig.h"
#include "Samples/LingotionGUISample/ThespeonDialogueOverlayWidget.h"
#include "AdvancedThespeonWidget.generated.h"

class UButton;
class UComboBoxString;
class UImage;
class UMultiLineEditableTextBox;
class USpinBox;
class UTextBlock;
class UTexture2D;
class UWidget;
class UWrapBox;
class UThespeonEmotionChipWidget;
class UThespeonEmotionEditorWidget;
class UThespeonDialogueOverlayWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAdvancedThespeonStateChanged);

/**
 * C++ controller for the LingotionGUISample (W_ThespeonGUI). The Widget Blueprint supplies layout and styling.
 *
 * Edits a line of dialogue as one or more segments (text, language, start/end emotion blend, start/end
 * speed and loudness), plus the character, module type and backend, and hands it to the hosted
 * UThespeonDialogueOverlayWidget for synthesis and playback. Ctrl+D toggles the multi-segment
 * developer controls, which are shown by default.
 */
UCLASS(Abstract, Blueprintable)
class LINGOTIONTHESPEON_API UAdvancedThespeonWidget : public UUserWidget
{
	GENERATED_BODY()

  public:
	/** Builds the model input from all Segments. The first segment supplies DefaultLanguage and DefaultEmotion (its dominant start emotion). */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Advanced GUI")
	FLingotionModelInput BuildModelInput() const;

	/** Builds the inference config from the selected backend and module type, with the input's defaults as fallbacks. */
	UFUNCTION(BlueprintPure, Category = "Lingotion Thespeon|Advanced GUI")
	FInferenceConfig BuildInferenceConfig() const;

	/** Applies an emotion map produced by the emotion editor modal to the start or end of the currently selected segment. */
	void ApplyEmotionMap(bool bForStart, const TMap<EEmotion, float>& EmotionMap);

	UPROPERTY(BlueprintAssignable, Category = "Lingotion Thespeon|Advanced GUI")
	FOnAdvancedThespeonStateChanged OnStateChanged;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI|State")
	FString CharacterName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI|State")
	EThespeonModuleType ModuleType = EThespeonModuleType::None;

	/**
	 * All segments of the line. Outside developer mode only the first is used; leaving developer mode merges
	 * every segment's text into the first and removes the rest. The first segment's speed and loudness are
	 * seeded from the spin boxes on construct.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon|Advanced GUI|State")
	TArray<FLingotionInputSegment> Segments;

	/** Index into Segments of the segment currently being edited by the GUI controls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI|State")
	int32 SelectedSegmentIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI|State")
	EBackendType BackendType = EBackendType::None;

  protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	// Preview (tunneling) so Ctrl+D is caught even while the input text box holds keyboard focus.
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Portrait per character name. Matched exactly first, then trimmed and case-insensitive. Falls back to DefaultCharacterPortrait; with neither,
	 * the image is hidden. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	TMap<FString, TSoftObjectPtr<UTexture2D>> CharacterPortraits;

	/** Used when the selected character has no entry in CharacterPortraits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	TSoftObjectPtr<UTexture2D> DefaultCharacterPortrait;

	/** Emotion editor modal class. If unset, W_ThespeonEmotionEditorModal is loaded by path. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	TSubclassOf<UThespeonEmotionEditorWidget> EmotionEditorClass;

	/** Prefix for synthesis session IDs, which take the form <Prefix>_<GUID>. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	FString SessionIdPrefix = TEXT("LingotionGUISample");

	/**
	 * Widget spawned once per emotion into the start/end summary WrapBoxes. Design a Blueprint subclass to style it graphically.
	 * If unset, no summary is shown. An empty blend shows a single None chip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	TSubclassOf<UThespeonEmotionChipWidget> EmotionChipClass;

	/** Maximum per-emotion chips before the rest fold into one "N more X%" chip, which is only used when 2 or more would be hidden. Minimum 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	int32 MaxSummaryChips = 3;

	/**
	 * Per-segment cap on characters in InputTextBox; anything typed or pasted beyond this is discarded. Zero disables the cap.
	 * Not enforced on text merged by deleting a segment or leaving developer mode.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lingotion Thespeon|Advanced GUI")
	int32 MaxInputTextLength = 250;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> CharacterComboBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> ModuleTypeComboBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> BackendComboBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> LanguageComboBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CharacterPortrait;
	// Optional; made SelfHitTestInvisible on construct.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> GlobalControls;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UMultiLineEditableTextBox> InputTextBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWrapBox> StartEmotionSummary;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> StartEmotionEditButton;
	// The designer values of the four speed/loudness spin boxes seed the first segment on construct.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USpinBox> StartSpeedSpinBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USpinBox> StartLoudnessSpinBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWrapBox> EndEmotionSummary;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> EndEmotionEditButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USpinBox> EndSpeedSpinBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USpinBox> EndLoudnessSpinBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SynthesizeButton;
	// Optional; shows submit/complete/fail status, in red for errors.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
	// The overlay that synthesizes and plays the line.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UThespeonDialogueOverlayWidget> DialogueOverlay;

	// Developer-mode multi-segment controls. Shown by default (see bDeveloperMode); toggled with Ctrl+D. Optional so the
	// Blueprint keeps compiling until the matching widgets are added to it.

	// Container holding the buttons/labels below; its visibility is what Ctrl+D toggles.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> SegmentControl;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PreviousSegmentButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> NextSegmentButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CreateSegmentButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> DeleteSegmentButton;

  private:
	UFUNCTION() void HandleCharacterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleModuleTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleBackendChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleLanguageChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void HandleTextChanged(const FText& Text);
	UFUNCTION() void HandleStartSpeedChanged(float Value);
	UFUNCTION() void HandleStartLoudnessChanged(float Value);
	UFUNCTION() void HandleEndSpeedChanged(float Value);
	UFUNCTION() void HandleEndLoudnessChanged(float Value);
	UFUNCTION() void OpenStartEmotionEditor();
	UFUNCTION() void OpenEndEmotionEditor();
	UFUNCTION() void Synthesize();
	UFUNCTION() void HandleDialogueEnded(EThespeonDialogueResult Result);
	UFUNCTION() void SelectPreviousSegment();
	UFUNCTION() void SelectNextSegment();
	UFUNCTION() void CreateSegment();
	UFUNCTION() void DeleteSegment();

	void PopulateCharacters();
	void PopulateModuleTypes();
	void PopulateBackends();
	void RefreshAll();
	void RefreshPortrait();
	UTexture2D* ResolvePortraitTexture() const;
	void RefreshControls();
	void RefreshEmotionSummaries();
	void RefreshLanguage();
	void OpenEmotionEditor(bool bForStart);
	void SetStatus(const FText& Message, bool bIsError = false);
	void LogSegment(const TCHAR* EditContext) const;

	/** Guarantees Segments has at least one entry and SelectedSegmentIndex is in range; returns the selected segment. */
	FLingotionInputSegment& GetSelectedSegment();
	/** Returns a static empty segment if Segments is empty (does not modify the array). */
	const FLingotionInputSegment& GetSelectedSegment() const;
	/** Shows/hides the developer-mode segment controls and, when hiding, collapses everything back into one segment. */
	void ToggleDeveloperMode();
	/** Updates visibility and enabled state of the segment navigation controls for the current mode/index. */
	void RefreshSegmentControls();
	FString GetSelectedModuleId() const;
	static FString LanguageLabel(const FLingotionLanguage& Language);

	/** Rebuilds Container's chips, dominant emotion first, up to MaxSummaryChips plus one overflow chip, reflowing instead of overflowing. */
	void BuildEmotionChips(UWrapBox* Container, const TMap<EEmotion, float>& EmotionMap);
	/** Spawns one EmotionChipClass instance for the given emotion/weight into Container. */
	void AddEmotionChip(UWrapBox* Container, EEmotion Emotion, float Weight);
	/** Spawns an EmotionChipClass instance configured as the "N more X%" overflow indicator into Container. */
	void AddOverflowChip(UWrapBox* Container, int32 HiddenCount, float CombinedWeight);

	TMap<FString, EThespeonModuleType> ModuleTypeOptions;
	TMap<FString, EBackendType> BackendOptions;
	TMap<EThespeonModuleType, FString> ModuleIds;
	TArray<FLingotionLanguage> AvailableLanguages;
	TMap<FString, int32> LanguageOptions;
	bool bRefreshingOptions = false;
	bool bRefreshingControls = false;
	/** When true, the multi-segment developer controls are visible. Toggled with Ctrl+D. */
	bool bDeveloperMode = true;
};
