// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "../../Public/Core/ModelInput.h"
#include "Core/LingotionLogger.h"
// FModelInput is now a struct with inline default constructor
// No additional implementation needed
bool FLingotionModelInput::ValidateAndPopulate(FLingotionModelInput& Input)
{
    if(Input.Segments.Num() == 0)
    {
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("No segments provided, adding a default empty segment."));
        return false;
    }
    if(Input.Segments.Num() > 1)
    {
        LINGO_LOG(EVerbosityLevel::Warning, TEXT("Multiple segments provided (%d). Currently only single segment is supported."), Input.Segments.Num());
        return false;
    }
    // Add further validation logic here
    return true;
}

bool FLingotionInputSegment::TryParseFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionInputSegment& OutSegment)
{
    if (!Json.IsValid())
        return false;

    return Json->TryGetStringField(TEXT("text"), OutSegment.Text);
}

bool FLingotionModelInput::TryParseInputFromJson(const TSharedPtr<FJsonObject>& Json, FLingotionModelInput& OutModelInput)
{
    if (!Json.IsValid())
        return false;

    bool bOk = true;

    bOk &= Json->TryGetStringField(TEXT("actorName"), OutModelInput.CharacterName);

    FString ModuleStr;
    if (Json->TryGetStringField(TEXT("moduleType"), ModuleStr))
        bOk &= ParseInputEnum<EThespeonModuleType>(ModuleStr, OutModelInput.ModuleType);

    FString EmotionStr;
    if (Json->TryGetStringField(TEXT("defaultEmotion"), EmotionStr))
        bOk &= ParseInputEnum<EEmotion>(EmotionStr, OutModelInput.DefaultEmotion);

    const TSharedPtr<FJsonObject>* LangObj = nullptr;
    if (Json->TryGetObjectField(TEXT("defaultLanguage"), LangObj))
        bOk &= FLingotionLanguage::TryParseFromJson(*LangObj, OutModelInput.DefaultLanguage);

    const TArray<TSharedPtr<FJsonValue>>* JsonSegments = nullptr;
    if (Json->TryGetArrayField(TEXT("segments"), JsonSegments))
    {
        OutModelInput.Segments.Reset();
        for (auto& Val : *JsonSegments)
        {
            if (!Val.IsValid() || !Val->AsObject().IsValid())
            {
                return false;
            }
            FLingotionInputSegment newSegment;
            bOk &= FLingotionInputSegment::TryParseFromJson(Val->AsObject(), newSegment);
            OutModelInput.Segments.Add(newSegment);
        }
    } 

    return bOk;
}
