// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "ModuleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Utils/SubsystemUtils.h"

void UModuleManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// your init...
}

void UModuleManager::Deinitialize()
{
	FWriteScopeLock WriteLock(ModulesLock);
	Modules.Empty();
	Super::Deinitialize();
}

UModuleManager* UModuleManager::Get()
{
	return FLingotionThespeonSubsystemUtils::GetModuleManager();
}

/**
 * @brief Deregisters a registered module if no workloads are loaded on any backend.
 * @param ModuleID - The ID of the module to deregister.
 * @return true if the module was successfully deregistered, false otherwise.
 */
bool UModuleManager::TryDeregisterModule(FString ModuleID)
{
	FWriteScopeLock WriteLock(ModulesLock);
	auto* ModulePtr = Modules.Find(ModuleID);
	if (!ModulePtr)
	{
		return true; // not registered anyway
	}
	if (!ModulePtr->Get()->RegisteredWorkloads.IsEmpty())
	{
		return false; // workloads still loaded, cannot deregister
	}
	Modules.Remove(ModuleID);
	LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Deregistered module '%s'."), *ModuleID);
	return true;
}

bool UModuleManager::IsRegistered(FString ModuleID)
{
	FReadScopeLock ReadLock(ModulesLock);
	return Modules.Contains(ModuleID);
}

// Collects the workload IDs a module recorded for the given requested backend, or across every
// backend it is registered on when BackendType is None.
static TSet<FString> GatherRecordedWorkloadIDs(const Thespeon::Core::Module& Module, EBackendType BackendType)
{
	if (BackendType != EBackendType::None)
	{
		const TSet<FString>* Recorded = Module.RegisteredWorkloads.Find(BackendType);
		return Recorded ? *Recorded : TSet<FString>();
	}

	TSet<FString> All;
	for (const auto& Pair : Module.RegisteredWorkloads)
	{
		All.Append(Pair.Value);
	}
	return All;
}

// Determines which workload IDs can be safely removed when unloading a module.
// Computes the set difference between this module's recorded workload IDs and those recorded by all
// other loaded modules of the same type, ensuring shared models are not prematurely removed.
//
// Both sides read what registration recorded rather than re-deriving IDs from the backend: a
// metagraph device pin can make a workload ID name a backend other than the requested one, so a
// re-derivation could not reproduce what was actually created.
TSet<FString> UModuleManager::GetWorkloadIDsToRemove(Thespeon::Core::Module* Module, EBackendType BackendType)
{
	if (BackendType != EBackendType::None && !Module->RegisteredWorkloads.Contains(BackendType))
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("Module '%s' is not loaded on backend '%s'. Nothing to do."),
		    *Module->ModuleID,
		    *UEnum::GetValueAsString(BackendType)
		);
		return TSet<FString>();
	}
	const TSet<FString> WorkloadIDs = GatherRecordedWorkloadIDs(*Module, BackendType);
	TSet<FString> OtherWorkloadIDs;

	// This module's own other registrations come first. A declared device makes a workload ID
	// identical no matter which backend was requested — the phonemizer is "CPU_<md5>" whether it was
	// preloaded for CPU or GPU — so unloading one backend must not remove a pool the module's other
	// registered backends are still using.
	if (BackendType != EBackendType::None)
	{
		for (const auto& Registration : Module->RegisteredWorkloads)
		{
			if (Registration.Key != BackendType)
			{
				OtherWorkloadIDs.Append(Registration.Value);
			}
		}
	}

	FReadScopeLock ReadLock(ModulesLock);
	for (const auto& LoadedModule : Modules)
	{
		// skip modules that are the same or of a different type. Other modules are compared across
		// every backend they are registered on, not just the one being unloaded: because a pinned
		// workload ID does not name the requested backend, a module loaded on CPU can legitimately
		// own an ID that a GPU unload would otherwise remove from under it.
		if (LoadedModule.Value->ModuleID == Module->ModuleID || LoadedModule.Value->GetModuleType() != Module->GetModuleType())
		{
			continue;
		}
		OtherWorkloadIDs.Append(GatherRecordedWorkloadIDs(*LoadedModule.Value, EBackendType::None));
	}

	return WorkloadIDs.Difference(OtherWorkloadIDs);
}

// Returns the set of language module IDs that are unique to the given character module
// (not referenced by any other loaded character module). Used during unload to determine
// which language modules can be safely removed alongside the character module.
TSet<FString> UModuleManager::GetNonOverlappingModelLangModules(Thespeon::Character::CharacterModule* Module)
{
	TArray<FString> langModuleIDs;
	Module->LanguageModuleIDs.GenerateValueArray(langModuleIDs);

	TSet<FString> moduleMd5s(langModuleIDs);
	TSet<FString> otherMd5s;

	FReadScopeLock ReadLock(ModulesLock);
	for (const auto& LoadedModule : Modules)
	{
		if (LoadedModule.Value->ModuleID == Module->ModuleID || LoadedModule.Value->GetModuleType() != Module->GetModuleType())
		{
			continue;
		}

		TArray<FString> currentLangModuleIDs;
		StaticCastSharedPtr<Thespeon::Character::CharacterModule>(LoadedModule.Value)->LanguageModuleIDs.GenerateValueArray(currentLangModuleIDs);
		otherMd5s.Append(currentLangModuleIDs);
	}

	return moduleMd5s.Difference(otherMd5s);
}
