// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "RuntimeLookupTable.h"
#include "Core/LingotionLogger.h"

Thespeon::Language::RuntimeLookupTable::RuntimeLookupTable(const TMap<FString, FString>& InStaticLookupTable)
    : StaticLookupTable(InStaticLookupTable), DynamicLookupTable()
{
}

bool Thespeon::Language::RuntimeLookupTable::TryGetValue(const FString& Key, FString& OutValue) const
{
	FReadScopeLock ReadLock(TableLock);
	// check generated/user-added table first
	if (DynamicLookupTable.Contains(Key))
	{
		OutValue = DynamicLookupTable[Key];
		return true;
	}
	if (StaticLookupTable.Contains(Key))
	{
		OutValue = StaticLookupTable[Key];
		return true;
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Lookup for key '%s' not found in either static or dynamic tables."), *Key);
	OutValue = TEXT("");
	return false;
}

bool Thespeon::Language::RuntimeLookupTable::ContainsKey(const FString& Key) const
{
	FReadScopeLock ReadLock(TableLock);
	return StaticLookupTable.Contains(Key) || DynamicLookupTable.Contains(Key);
}

void Thespeon::Language::RuntimeLookupTable::AddOrUpdateDynamicEntry(const FString& Key, const FString& Value)
{
	FWriteScopeLock WriteLock(TableLock);
	if (DynamicLookupTable.Contains(Key))
	{
		DynamicLookupTable[Key] = Value;
	}
	else
	{
		DynamicLookupTable.Add(Key, Value);
	}
}
