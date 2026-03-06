// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "LookupTableManager.h"
#include "LanguageModule.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Core/LingotionLogger.h"
#include "Engine/World.h"
#include "Utils/SubsystemUtils.h"

void ULookupTableManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("LookupTableManager initialized."));
}

void ULookupTableManager::Deinitialize()
{
	DisposeAndClear();
	Super::Deinitialize();
}

ULookupTableManager* ULookupTableManager::Get()
{
	// Only ever do this on game thread
	check(IsInGameThread());

	return FLingotionThespeonSubsystemUtils::GetLookupTableManager();
}

void ULookupTableManager::RegisterLookupTable(Thespeon::Language::LanguageModule* Module)
{
	if (!Module)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Module is null"));
		return;
	}

	FString MD5 = Module->GetLookupTableID();

	if (IsRegistered(MD5))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Lookup table with MD5 '%s' already registered."), *MD5);
		return;
	}

	// Load the lookup table from the module
	TMap<FString, FString> LookupTableData = Module->GetLookupTable();

	// Create a new RuntimeLookupTable with the loaded data
	TUniquePtr<Thespeon::Language::RuntimeLookupTable> LookupTable = MakeUnique<Thespeon::Language::RuntimeLookupTable>(LookupTableData);

	AvailableLookupTables.Add(MD5, MoveTemp(LookupTable));

	LINGO_LOG_FUNC(
	    EVerbosityLevel::Info, TEXT("Successfully registered lookup table with MD5 '%s' containing %d entries."), *MD5, LookupTableData.Num()
	);
}

bool ULookupTableManager::TryDeregisterTable(Thespeon::Language::LanguageModule* Module)
{

	FString MD5 = Module->GetLookupTableID();

	if (!IsRegistered(MD5))
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("Lookup table with MD5 '%s' not registered."), *MD5);
		return false; // not registered, cancel unload
	}
	if (Module->LoadedBackends.Num() != 0)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Lookup table with MD5 '%s' still has loaded workloads."), *MD5);
		return true; // workloads still loaded, cannot deregister but dont cancel unload
	}

	AvailableLookupTables.Remove(MD5);
	LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Successfully deregistered lookup table with MD5 '%s'."), *MD5);
	return true;
}

Thespeon::Language::RuntimeLookupTable* ULookupTableManager::GetLookupTable(const FString& MD5)
{
	TUniquePtr<Thespeon::Language::RuntimeLookupTable>* LookupTablePtr = AvailableLookupTables.Find(MD5);

	if (LookupTablePtr && LookupTablePtr->IsValid())
	{
		return LookupTablePtr->Get();
	}
	else
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Lookup table with MD5: %s not found."), *MD5);
		return nullptr;
	}
}

void ULookupTableManager::DisposeAndClear()
{
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Clearing %d lookup tables."), AvailableLookupTables.Num());
	AvailableLookupTables.Empty();
}

bool ULookupTableManager::IsRegistered(const FString& MD5) const
{
	return AvailableLookupTables.Contains(MD5);
}
