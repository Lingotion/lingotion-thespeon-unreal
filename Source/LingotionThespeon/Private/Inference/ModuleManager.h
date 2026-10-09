// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

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
			if (auto* Existing = Modules.Find(ModuleEntry.ModuleID))
			{
				return CastModuleChecked<T>(*Existing);
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
			// Still checked: another thread may have won the race and registered this ID as a
			// different module type, in which case AddModuleIfAbsent_Locked kept theirs.
			return CastModuleChecked<T>(Modules[ModuleEntry.ModuleID]);
		}
	}
	// Resource overlap detection - compares GetModuleType() discriminators instead of an enum, since
	// Module is not a UObject (no Cast<T>) and RTTI is disabled (no dynamic_cast).
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
	/**
	 * Downcasts a stored module to T only if it really is a T.
	 *
	 * Modules are stored type-erased as TSharedPtr<Module>, and StaticCastSharedPtr performs no
	 * check of its own — two call sites requesting the same ModuleID with different T would
	 * otherwise be silent undefined behaviour. Compares the module's GetModuleType() discriminator
	 * against T::StaticModuleType(), which is the only type identity available here: Module is a
	 * plain C++ class rather than a UObject, so Cast<T> does not apply, and RTTI is disabled in
	 * LingotionThespeon.Build.cs, so dynamic_cast is unavailable.
	 *
	 * Caller must hold either lock, so the shared pointer stays alive for the duration.
	 *
	 * @return The module as T, or nullptr if it is null or of a different type.
	 */
	template <typename T>
	static TSharedPtr<T, ESPMode::ThreadSafe> CastModuleChecked(const TSharedPtr<Thespeon::Core::Module, ESPMode::ThreadSafe>& StoredModule)
	{
		static_assert(std::is_base_of_v<Thespeon::Core::Module, T>, "T must derive from Module");
		if (!StoredModule.IsValid())
		{
			return nullptr;
		}
		const FString StoredType = StoredModule->GetModuleType();
		const FString RequestedType = T::StaticModuleType();
		if (StoredType != RequestedType)
		{
			LINGO_LOG(
			    EVerbosityLevel::Error,
			    TEXT("Module '%s' is registered as type '%s' but was requested as type '%s'. Refusing to cast."),
			    *StoredModule->ModuleID,
			    *StoredType,
			    *RequestedType
			);
			return nullptr;
		}
		return StaticCastSharedPtr<T>(StoredModule);
	}

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
