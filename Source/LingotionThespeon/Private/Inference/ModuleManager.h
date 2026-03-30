// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Module.h"
#include "Character/CharacterModule.h"
#include "Core/LingotionLogger.h"
#include "Core/BackendType.h"
#include "HAL/CriticalSection.h"
#include "ModuleManager.generated.h"

/**
 * Game instance subsystem that owns and manages all loaded Module instances (character and language).
 *
 * Used during preload/unload to register, look up, and deregister modules.
 * Also provides overlap detection so the WorkloadManager knows which workloads are safe to remove.
 */
UCLASS()
class UModuleManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()
  public:
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	static UModuleManager* Get();

	template <typename T> void RegisterModule(Thespeon::Core::FModuleEntry ModuleEntry)
	{
		static_assert(std::is_base_of_v<Thespeon::Core::Module, T>, "T must derive from Module");
		FWriteScopeLock WriteLock(ModulesLock);
		AddModuleIfAbsent_Locked<T>(ModuleEntry);
	}
	bool TryDeregisterModule(FString ModuleID);
	bool IsRegistered(FString ModuleID);
	template <typename T> TSharedPtr<T, ESPMode::ThreadSafe> GetModule(Thespeon::Core::FModuleEntry ModuleEntry, bool ShouldCreate = true)
	{
		if (ModuleEntry.ModuleID.IsEmpty())
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Invalid ModuleEntry provided."));
			return nullptr;
		}
		static_assert(std::is_base_of_v<Thespeon::Core::Module, T>, "T must derive from Module");

		{
			FReadScopeLock ReadLock(ModulesLock);
			if (Modules.Contains(ModuleEntry.ModuleID))
			{
				return StaticCastSharedPtr<T>(Modules[ModuleEntry.ModuleID]);
			}
		}
		// Not found — need to create
		if (!ShouldCreate)
		{
			LINGO_LOG(EVerbosityLevel::Error, TEXT("Module '%s' not found."), *ModuleEntry.ModuleID);
			return nullptr;
		}
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Module '%s' not found. Creating "), *ModuleEntry.ModuleID);
		{
			FWriteScopeLock WriteLock(ModulesLock);
			// Double-check after acquiring write lock
			AddModuleIfAbsent_Locked<T>(ModuleEntry);
			return StaticCastSharedPtr<T>(Modules[ModuleEntry.ModuleID]);
		}
	}
	// Resource overlap detection - now uses safe dynamic_cast instead of enum + static_cast
	TSet<FString> GetWorkloadIDsToRemove(
	    Thespeon::Core::Module* Module,
	    EBackendType BackendType
	); // returns file md5s that are not in any other module (safe to remove)

	/**
	 * Finds language modules used by the given character module that are not used by any other character module.
	 * Used to determine which language modules can be safely removed when a character module is removed.
	 */
	TSet<FString> GetNonOverlappingModelLangModules(Thespeon::Character::CharacterModule* Module);

  private:
	/** Creates a module if not already present. Caller must hold the write lock. */
	template <typename T> void AddModuleIfAbsent_Locked(const Thespeon::Core::FModuleEntry& ModuleEntry)
	{
		static_assert(std::is_base_of_v<Thespeon::Core::Module, T>, "T must derive from Module");
		if (!Modules.Contains(ModuleEntry.ModuleID))
		{
			Modules.Add(ModuleEntry.ModuleID, MakeShared<T, ESPMode::ThreadSafe>(ModuleEntry));
			LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Registered module '%s'."), *ModuleEntry.ModuleID);
		}
	}

	TMap<FString, TSharedPtr<Thespeon::Core::Module, ESPMode::ThreadSafe>> Modules;
	mutable FRWLock ModulesLock;
};
