// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

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
	if (!ModulePtr->Get()->LoadedBackends.IsEmpty())
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

// Determines which workload IDs can be safely removed when unloading a module.
// Computes the set difference between this module's workload IDs and the IDs used by all
// other loaded modules of the same type, ensuring shared models are not prematurely removed.
TSet<FString> UModuleManager::GetWorkloadIDsToRemove(Thespeon::Core::Module* Module, EBackendType BackendType)
{
	if (BackendType != EBackendType::None && !Module->LoadedBackends.Contains(BackendType))
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("Module '%s' is not loaded on backend '%s'. Nothing to do."),
		    *Module->ModuleID,
		    *UEnum::GetValueAsString(BackendType)
		);
		return TSet<FString>();
	}
	TSet<FString> WorkloadIDs;
	if (!Module->AddLoadedWorkloadIDs(BackendType, WorkloadIDs))
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get loaded workload IDs for module '%s'."), *Module->ModuleID);
		return TSet<FString>();
	}
	TSet<FString> OtherWorkloadIDs;

	FReadScopeLock ReadLock(ModulesLock);
	for (const auto& LoadedModule : Modules)
	{
		// skip modules that are the same, of different type, or not loaded on the backend in question
		if (LoadedModule.Value->ModuleID == Module->ModuleID || LoadedModule.Value->GetModuleType() != Module->GetModuleType() ||
		    (BackendType != EBackendType::None && !LoadedModule.Value->LoadedBackends.Contains(BackendType)))
		{
			continue;
		}
		if (!LoadedModule.Value->AddLoadedWorkloadIDs(BackendType, OtherWorkloadIDs))
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get loaded workload IDs for module '%s'."), *LoadedModule.Value->ModuleID);
			return TSet<FString>();
		}
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
