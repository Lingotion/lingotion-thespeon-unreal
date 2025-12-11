// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "RuntimeLookupTable.h"
#include "Core/LingotionLogger.h"

Thespeon::LanguagePack::RuntimeLookupTable::RuntimeLookupTable(const TMap<FString, FString>& InStaticLookupTable)
    : StaticLookupTable(InStaticLookupTable)
    , DynamicLookupTable()
{
}

bool Thespeon::LanguagePack::RuntimeLookupTable::TryGetValue(const FString& Key, FString& OutValue) const
{
    // Check static table first, then dynamic - matching Unity behavior
    // After user-provided entries are added, dynamic should be checked first
    if (StaticLookupTable.Contains(Key))
    {
        OutValue = StaticLookupTable[Key];
        return true;
    }
    if (DynamicLookupTable.Contains(Key))
    {
        OutValue = DynamicLookupTable[Key];
        return true;
    }
    
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("Lookup for key '%s' not found in either static or dynamic tables."), *Key);
    OutValue = TEXT("");
    return false;
}

bool Thespeon::LanguagePack::RuntimeLookupTable::ContainsKey(const FString& Key) const
{
    return StaticLookupTable.Contains(Key) || DynamicLookupTable.Contains(Key);
}

void Thespeon::LanguagePack::RuntimeLookupTable::AddOrUpdateDynamicEntry(const FString& Key, const FString& Value)
{
    if (DynamicLookupTable.Contains(Key))
    {
        DynamicLookupTable[Key] = Value;
    }
    else
    {
        DynamicLookupTable.Add(Key, Value);
    }
}
