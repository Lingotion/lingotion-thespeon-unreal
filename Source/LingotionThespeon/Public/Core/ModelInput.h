// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h" // optional, for safety with macros
#include "Core/Language.h"
#include "ModelInput.generated.h"

/**
 * Quality tier of a Thespeon character module. Higher tiers produce better audio at the cost of increased computation.
 */
UENUM(BlueprintType)
enum class EThespeonModuleType : uint8
{
	/** No module type selected. Synthesize uses the InferenceConfig's ModuleType (or the first imported one) instead; preload and unload calls fail.
	 */
	None UMETA(DisplayName = "None"),
	/** Ultra-high quality. Best fidelity, highest resource usage. */
	XL UMETA(DisplayName = "XL"),
	/** High quality. High fidelity, high resource usage. */
	L UMETA(DisplayName = "L"),
	/** Medium quality. Balanced fidelity and performance. */
	M UMETA(DisplayName = "M"),
	/** Low quality. Faster inference, reduced fidelity. */
	S UMETA(DisplayName = "S"),
	/** Ultra-low quality. Fastest inference, lowest fidelity. */
	XS UMETA(DisplayName = "XS")
};

/**
 * Enumeration representing various emotions that can be associated with a segment.
 * Also contains a None as a special null-like value.
 */
UENUM(BlueprintType)
enum class EEmotion : uint8
{
	/** No emotion. Special null-like value. */
	None = 0 UMETA(DisplayName = "None"),
	/** Delighted, giddy. Abundance of energy. Message: This is better than I imagined. */
	Ecstasy = 1 UMETA(DisplayName = "Ecstasy"),
	/** Connected, proud. Glowing sensation. Message: I want to support the person or thing. */
	Admiration = 2 UMETA(DisplayName = "Admiration"),
	/** Alarmed, petrified. Hard to breathe. Message: There is big danger. */
	Terror = 3 UMETA(DisplayName = "Terror"),
	/** Inspired, WOWed. Heart stopping sensation. Message: Something is totally unexpected. */
	Amazement = 4 UMETA(DisplayName = "Amazement"),
	/** Heartbroken, distraught. Hard to get up. Message: Love is lost. */
	Grief = 5 UMETA(DisplayName = "Grief"),
	/** Disturbed, horrified. Bileous and vehement sensation. Message: Fundamental values are violated. */
	Loathing = 6 UMETA(DisplayName = "Loathing"),
	/** Overwhelmed, furious. Pounding heart, seeing red. Message: I am blocked from something vital. */
	Rage = 7 UMETA(DisplayName = "Rage"),
	/** Intense, focused. Highly focused sensation. Message: Something big is coming. */
	Vigilance = 8 UMETA(DisplayName = "Vigilance"),
	/** Excited, pleased. Sense of energy and possibility. Message: Life is going well. */
	Joy = 9 UMETA(DisplayName = "Joy"),
	/** Accepting, safe. Warm sensation. Message: This is safe. */
	Trust = 10 UMETA(DisplayName = "Trust"),
	/** Stressed, scared. Agitated sensation. Message: Something I care about is at risk. */
	Fear = 11 UMETA(DisplayName = "Fear"),
	/** Shocked, unexpected. Heart pounding. Message: Something new happened. */
	Surprise = 12 UMETA(DisplayName = "Surprise"),
	/** Bummed, loss. Heavy sensation. Message: Love is going away. */
	Sadness = 13 UMETA(DisplayName = "Sadness"),
	/** Distrust, rejecting. Bitter and unwanted sensation. Message: Rules are violated. */
	Disgust = 14 UMETA(DisplayName = "Disgust"),
	/** Mad, fierce. Strong and heated sensation. Message: Something is in the way. */
	Anger = 15 UMETA(DisplayName = "Anger"),
	/** Curious, considering. Alert and exploring. Message: Change is happening. */
	Anticipation = 16 UMETA(DisplayName = "Anticipation"),
	/** Calm, peaceful. Relaxed, open-hearted. Message: Something essential or pure is happening. */
	Serenity = 17 UMETA(DisplayName = "Serenity"),
	/** Open, welcoming. Peaceful sensation. Message: We are in this together. */
	Acceptance = 18 UMETA(DisplayName = "Acceptance"),
	/** Worried, anxious. Cannot relax. Message: There could be a problem. */
	Apprehension = 19 UMETA(DisplayName = "Apprehension"),
	/** Scattered, uncertain. Unfocused sensation. Message: I don't know what to prioritize. */
	Distraction = 20 UMETA(DisplayName = "Distraction"),
	/** Blue, unhappy. Slow and disconnected. Message: Love is distant. */
	Pensiveness = 21 UMETA(DisplayName = "Pensiveness"),
	/** Tired, uninterested. Drained, low energy. Message: The potential for this situation is not being met. */
	Boredom = 22 UMETA(DisplayName = "Boredom"),
	/** Frustrated, prickly. Slightly agitated. Message: Something is unresolved. */
	Annoyance = 23 UMETA(DisplayName = "Annoyance"),
	/** Open, looking. Mild sense of curiosity. Message: Something useful might come. */
	Interest = 24 UMETA(DisplayName = "Interest"),
	/** Detached, apathetic. No sensation or feeling at all. Message: This does not affect me. */
	Emotionless = 25 UMETA(DisplayName = "Emotionless"),
	/** Distaste, scorn. Angry and sad at the same time. Message: This is beneath me. */
	Contempt = 26 UMETA(DisplayName = "Contempt"),
	/** Guilt, regret, shame. Disgusted and sad at the same time. Message: I regret my actions. */
	Remorse = 27 UMETA(DisplayName = "Remorse"),
	/** Dislike, displeasure. Sad and surprised. Message: This violates my values. */
	Disapproval = 28 UMETA(DisplayName = "Disapproval"),
	/** Astonishment, wonder. Surprise with a hint of fear. Message: This is overwhelming. */
	Awe = 29 UMETA(DisplayName = "Awe"),
	/** Obedience, compliance. Fearful but trusting. Message: I must follow this authority. */
	Submission = 30 UMETA(DisplayName = "Submission"),
	/** Cherish, treasure. Joy with trust. Message: I want to be with this person. */
	Love = 31 UMETA(DisplayName = "Love"),
	/** Cheerfulness, hopeful. Joyful anticipation. Message: Things will work out. */
	Optimism = 32 UMETA(DisplayName = "Optimism"),
	/** Pushy, self-assertive. Driven by anger. Message: I must remove obstacles. */
	Aggressiveness = 33 UMETA(DisplayName = "Aggressiveness")
};

/**
 * A single segment of text input for synthesis, with its own emotion and language.
 *
 * Multiple segments can be combined in FLingotionModelInput to produce speech
 * with per-segment emotion and language control.
 */
USTRUCT(BlueprintType)
struct FLingotionInputSegment
{
	GENERATED_BODY()
  public:
	/** The text content to synthesize. May include control characters (Pause, AudioSampleRequest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true), Category = "Lingotion Thespeon")
	FString Text;

	/** Legacy single emotion. Not read by inference: set StartEmotion and EndEmotion instead. The constructor that takes an EEmotion copies it into
	 * both. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	EEmotion Emotion = EEmotion::None;

	/** The emotion blend at the start of this segment. Keys are emotions, values are their intensities. Weights are clamped to [0, 1], None keys are
	 * removed, and the rest is normalized to sum to 1. The default {None: 1} means unset: the value is interpolated from neighbouring segments, or
	 * DefaultEmotion is used if no segment sets one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	TMap<EEmotion, float> StartEmotion;

	/** The emotion blend at the end of this segment. Keys are emotions, values are their intensities. Weights are clamped to [0, 1], None keys are
	 * removed, and the rest is normalized to sum to 1. The default {None: 1} means unset: the value is interpolated from neighbouring segments, or
	 * DefaultEmotion is used if no segment sets one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	TMap<EEmotion, float> EndEmotion;

	/** The language/dialect of this segment. If undefined, or not spoken by the character, the parent FLingotionModelInput's DefaultLanguage is used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	FLingotionLanguage Language;

	/** When true, the Text is treated as a custom phonetic pronunciation (IPA) rather than normal text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	bool bIsCustomPronounced = false;

	/** Speech-rate multiplier at the start of this segment (1.0 = normal). Interpolated linearly to EndSpeed within the segment. Not range-checked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	float StartSpeed = 1.0f;
	/** Speech-rate multiplier at the end of this segment (1.0 = normal). Not range-checked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	float EndSpeed = 1.0f;
	/** Loudness multiplier at the start of this segment (1.0 = normal). Interpolated linearly to EndLoudness within the segment. Not range-checked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	float StartLoudness = 1.0f;
	/** Loudness multiplier at the end of this segment (1.0 = normal). Not range-checked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	float EndLoudness = 1.0f;

	FLingotionInputSegment()
	{
		StartEmotion.Add(EEmotion::None, 1.0f);
		EndEmotion.Add(EEmotion::None, 1.0f);
	}

	FLingotionInputSegment(
	    const FString& InText,
	    TMap<EEmotion, float> InStartEmotion,
	    TMap<EEmotion, float> InEndEmotion,
	    const FLingotionLanguage& InLanguage,
	    bool bInIsCustomPronounced,
	    float InStartSpeed = 1.0f,
	    float InEndSpeed = 1.0f,
	    float InStartLoudness = 1.0f,
	    float InEndLoudness = 1.0f
	)
	    : Text(InText)
	    , StartEmotion(InStartEmotion)
	    , EndEmotion(InEndEmotion)
	    , Language(InLanguage)
	    , bIsCustomPronounced(bInIsCustomPronounced)
	    , StartSpeed(InStartSpeed)
	    , EndSpeed(InEndSpeed)
	    , StartLoudness(InStartLoudness)
	    , EndLoudness(InEndLoudness)
	{
	}

	/** Legacy constructor: sets Emotion and uses it as a full-weight StartEmotion and EndEmotion. */
	FLingotionInputSegment(
	    const FString& InText,
	    EEmotion InEmotion,
	    const FLingotionLanguage& InLanguage,
	    bool bInIsCustomPronounced,
	    float InStartSpeed = 1.0f,
	    float InEndSpeed = 1.0f,
	    float InStartLoudness = 1.0f,
	    float InEndLoudness = 1.0f
	)
	    : Text(InText)
	    , Emotion(InEmotion)
	    , Language(InLanguage)
	    , bIsCustomPronounced(bInIsCustomPronounced)
	    , StartSpeed(InStartSpeed)
	    , EndSpeed(InEndSpeed)
	    , StartLoudness(InStartLoudness)
	    , EndLoudness(InEndLoudness)
	{
		StartEmotion.Reset();
		StartEmotion.Add(InEmotion, 1.0f);
		EndEmotion = StartEmotion;
	}

	/**
	 * @brief Attempts to parse an input segment from a JSON object. Only "text" is read; other fields keep their defaults.
	 *
	 * @param Json The JSON object to parse.
	 * @param OutSegment Receives the parsed segment on success.
	 * @return true if parsing succeeded.
	 */
	static bool TryParseFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionInputSegment& OutSegment);
};

/**
 * Complete input for a Thespeon speech synthesis request.
 *
 * Contains one or more text segments, each with optional per-segment emotion and language overrides,
 * plus the character, module type, and default emotion/language that apply to the entire request.
 */
USTRUCT(BlueprintType)
struct FLingotionModelInput
{
	GENERATED_BODY()

  public:
	/** Ordered list of text segments to synthesize. Each segment can have its own emotion and language. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	TArray<FLingotionInputSegment> Segments;

	/** Which module type of the character to use for synthesis. If None or not imported, InferenceConfig.ModuleType or the first imported type is
	 * used, with a warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	EThespeonModuleType ModuleType;

	/** Name of the Thespeon character to use for synthesis. If empty or not imported, the first imported character is used, with a warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	FString CharacterName;

	/** Emotion used for the whole line when no segment sets an emotion. Otherwise segments without one interpolate from their neighbours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	EEmotion DefaultEmotion;

	/** Default language applied to segments that do not specify their own language. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	FLingotionLanguage DefaultLanguage;

	FLingotionModelInput() : ModuleType(EThespeonModuleType::None), DefaultEmotion(EEmotion::None) {}

	/** Parses an enum value from its name string. Returns false if the name is unknown. */
	template <typename TEnum> static bool ParseInputEnum(const FString& Str, TEnum& OutEnum)
	{
		UEnum* Enum = StaticEnum<TEnum>();
		if (!Enum)
		{
			return false;
		}

		int64 Value = Enum->GetValueByNameString(Str);
		if (Value == INDEX_NONE)
		{
			return false;
		}

		OutEnum = static_cast<TEnum>(Value);
		return true;
	}

	/**
	 * @brief Validates that the selected character module (character name + module type) has been imported into the project and sets a fallback
	 * character module if not.
	 * @param FallbackModuleType The module type to fall back to if the current one is invalid but the character exists.
	 * An unknown or empty CharacterName is replaced by the first imported character.
	 * @return true if the character module is valid or a fallback was set, false if no valid character module could be found.
	 */
	bool ValidateCharacterModule(EThespeonModuleType FallbackModuleType);
	/**
	 * @brief Validates that an entire input instance contains valid selections for currently loaded character modules.
	 * If any part is invalid, it will attempt to set fallbacks based on what is available. Then populates every
	 * segment's emotion, speed and loudness keypoints.
	 *
	 * Segment text is not changed: text preprocessing (normalization and numbers) happens at synthesis, with the
	 * rules of each segment's language pack.
	 *
	 * @param FallbackModuleType Module type to fall back to if the selected one is unavailable.
	 * @param FallbackLanguage Replaces an undefined DefaultLanguage on the input.
	 * @param FallbackEmotion Replaces a None DefaultEmotion on the input.
	 * @return true if the input is valid or was corrected with fallbacks; false if the character module is not loaded
	 *         or a segment's text is empty.
	 */
	bool ValidateAndPopulate(EThespeonModuleType FallbackModuleType, FLingotionLanguage FallbackLanguage, EEmotion FallbackEmotion);

	/**
	 * @brief Validates the input and sets fallbacks as ValidateAndPopulate does, without populating keypoints.
	 *
	 * Synthesis uses this: it populates the keypoints after text preprocessing, which can split segments and change
	 * their length.
	 *
	 * @param FallbackModuleType Module type to fall back to if the selected one is unavailable.
	 * @param FallbackLanguage Language to fall back to for segments with undefined languages.
	 * @param FallbackEmotion Emotion to fall back to for segments with None emotion.
	 * @return true if the input is valid or was successfully corrected with fallbacks.
	 */
	bool Validate(EThespeonModuleType FallbackModuleType, FLingotionLanguage FallbackLanguage, EEmotion FallbackEmotion);

	/**
	 * @brief Attempts to parse a complete model input from a JSON object. CharacterName is read from "actorName",
	 * and segments only read "text". On failure OutModelInput may be partly filled.
	 *
	 * @param Json The JSON object containing model input fields.
	 * @param OutModelInput Receives the parsed model input on success.
	 * @return true if parsing succeeded.
	 */
	static bool TryParseInputFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionModelInput& OutModelInput);

	/**
	 * @brief Serializes this model input to a JSON string.
	 *
	 * @return A JSON representation of the model input.
	 */
	FString ToJson() const;

  private:
	bool SetCharacterNameIfInvalid(class UManifestHandler* Manifest);
	bool SetModuleTypeIfInvalid(class UManifestHandler* Manifest, EThespeonModuleType FallbackModuleType);
	TArray<FLingotionLanguage> GetCandidateLanguages(class UManifestHandler* Manifest, class UModuleManager* ModuleManager);
	FLingotionLanguage ResolveSegmentLanguage(const FLingotionInputSegment& Segment, const TArray<FLingotionLanguage>& CandidateLanguages) const;
};
namespace Thespeon
{
namespace ControlCharacters
{
/**
 * This character tells Thespeon to insert a short silence in the generated dialogue.
 */
constexpr TCHAR Pause = TEXT('⏸');
/**
 * Thespeon can find the audio sample that best corresponds to a position in the input text. Place this character in the text to request
 * the audio sample index at that position. The OnAudioSampleRequestReceived delegate delivers all such sample indices in left-to-right order.
 * It is guaranteed to broadcast before the first OnAudioReceived for the same synthesis session.
 */
constexpr TCHAR AudioSampleRequest = TEXT('◎');
} // namespace ControlCharacters
} // namespace Thespeon
