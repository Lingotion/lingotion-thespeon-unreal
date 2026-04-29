// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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

	// Fast path: avoids expensive disk I/O if already registered.
	if (IsRegistered(MD5))
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Lookup table with MD5 '%s' already registered."), *MD5);
		return;
	}

	// Slow path: load lookup table data from disk (outside lock — may be expensive).
	TMap<FString, FString> LookupTableData = Module->GetLookupTable();
	TSharedPtr<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe> LookupTable =
	    MakeShared<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe>(LookupTableData);

	// Double-check under write lock: another thread may have registered the same table
	// between the read check above and this write lock. Mirrors the pattern in
	// UModuleManager::GetModule<T>() (ModuleManager.h:62-66).
	// Note: Cannot call IsRegistered() here — it acquires a read lock internally,
	// and FRWLock is not re-entrant. Doing so while holding the write lock would deadlock.
	{
		FWriteScopeLock WriteLock(LookupTablesLock);
		if (!AvailableLookupTables.Contains(MD5))
		{
			AvailableLookupTables.Add(MD5, MoveTemp(LookupTable));
		}
	}

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

	{
		FWriteScopeLock WriteLock(LookupTablesLock);
		AvailableLookupTables.Remove(MD5);
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Successfully deregistered lookup table with MD5 '%s'."), *MD5);
	return true;
}

TSharedPtr<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe> ULookupTableManager::GetLookupTable(const FString& MD5)
{
	FReadScopeLock ReadLock(LookupTablesLock);
	TSharedPtr<Thespeon::Language::RuntimeLookupTable, ESPMode::ThreadSafe>* Found = AvailableLookupTables.Find(MD5);

	if (Found && Found->IsValid())
	{
		return *Found;
	}
	else
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Lookup table with MD5: %s not found."), *MD5);
		return nullptr;
	}
}

void ULookupTableManager::DisposeAndClear()
{
	FWriteScopeLock WriteLock(LookupTablesLock);
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Clearing %d lookup tables."), AvailableLookupTables.Num());
	AvailableLookupTables.Empty();
}

bool ULookupTableManager::IsRegistered(const FString& MD5) const
{
	FReadScopeLock ReadLock(LookupTablesLock);
	return AvailableLookupTables.Contains(MD5);
}
