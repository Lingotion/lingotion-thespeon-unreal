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
	/** No module type selected. */
	None UMETA(DisplayName = "None"),
	/** Ultra-high quality. Best fidelity, highest resource usage. */
	XL UMETA(DisplayName = "XL"),
	/** High quality. */
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

	/** The emotion to apply to this segment. None uses the default emotion from the parent FLingotionModelInput. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	EEmotion Emotion;

	/** The language/dialect of this segment. Undefined uses the default language from the parent FLingotionModelInput. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	FLingotionLanguage Language;

	/** When true, the Text is treated as a custom phonetic pronunciation (IPA) rather than normal text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	bool bIsCustomPronounced = false;

	FLingotionInputSegment() : Emotion(EEmotion::None) {}

	FLingotionInputSegment(const FString& InText, EEmotion InEmotion, const FLingotionLanguage& InLanguage, bool bInIsCustomPronounced)
	    : Text(InText), Emotion(InEmotion), Language(InLanguage), bIsCustomPronounced(bInIsCustomPronounced)
	{
	}

	/**
	 * @brief Attempts to parse an input segment from a JSON object.
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

	/** Which module type of the current Thespeon character to use for synthesis. Must match an imported character module. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	EThespeonModuleType ModuleType;

	/** Name of the Thespeon character to use for synthesis. Must match an imported character module. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	FString CharacterName;

	/** Default emotion applied to segments that do not specify their own emotion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	EEmotion DefaultEmotion;

	/** Default language applied to segments that do not specify their own language. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lingotion Thespeon")
	FLingotionLanguage DefaultLanguage;

	FLingotionModelInput()
	    : ModuleType(EThespeonModuleType::None) // Set appropriate default
	    , DefaultEmotion(EEmotion::None)
	{
	}

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
	 * @return true if the character module is valid or a fallback was set, false if no valid character module could be found.
	 */
	bool ValidateCharacterModule(EThespeonModuleType FallbackModuleType);
	/**
	 * @brief Validates that an entire input instance contains valid selections for currently loaded character modules.
	 * If any part is invalid, it will attempt to set fallbacks based on what is available.
	 *
	 * @param FallbackModuleType Module type to fall back to if the selected one is unavailable.
	 * @param FallbackLanguage Language to fall back to for segments with undefined languages.
	 * @param FallbackEmotion Emotion to fall back to for segments with None emotion.
	 * @return true if the input is valid or was successfully corrected with fallbacks.
	 */
	bool ValidateAndPopulate(EThespeonModuleType FallbackModuleType, FLingotionLanguage FallbackLanguage, EEmotion FallbackEmotion);

	/**
	 * @brief Attempts to parse a complete model input from a JSON object.
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
 * This character tells Thespeon to insert a short pause of silence in the generated dialogue.
 */
constexpr TCHAR Pause = TEXT('⏸');
/**
 * Thespeon is able to find the audio sample which best corresponds to a position in the input text. This character marks one such position to request
 * its corresponding audio sample. The FOnAudioSampleRequestReceived delegate will deliver all such sample indices in left-to-right order.
 * It is guaranteed to broadcast before the first FOnAudioReceived for the same synthesis session.
 */
constexpr TCHAR AudioSampleRequest = TEXT('◎');
} // namespace ControlCharacters
} // namespace Thespeon
