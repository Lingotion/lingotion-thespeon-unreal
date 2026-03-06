// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "NumberConverter.h"
#include "NumberToWordsEnglish.h"
#include "Core/LingotionLogger.h"

using namespace Thespeon::Language;
const TMap<FString, FNumberConverterFactory::FFactoryFunc>& FNumberConverterFactory::GetRegistry()
{
	static const TMap<FString, FFactoryFunc> Registry = {
	    {TEXT("eng"), MakeFactory<FNumberToWordsEnglish>()},
	    // Add new languages here - just specify the class type:
	    // { TEXT("swe"), MakeFactory<FNumberToWordsSwedish>() },
	};
	return Registry;
}

TUniquePtr<FNumberConverter> FNumberConverterFactory::Create(const FString& Iso639_2)
{
	const FString LowerLang = Iso639_2.ToLower();
	const TMap<FString, FFactoryFunc>& Registry = GetRegistry();

	if (const FFactoryFunc* Factory = Registry.Find(LowerLang))
	{
		return (*Factory)();
	}

	LINGO_LOG(EVerbosityLevel::Warning, TEXT("Language '%s' is not supported for number conversion"), *Iso639_2);
	return nullptr;
}
