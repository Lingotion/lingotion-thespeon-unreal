#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Inference/InferenceWorkloadManager.h"
#include "Inference/ModuleManager.h"
#include "Language/LookupTableManager.h"

/**
 * Utility functions for accessing LingotionThespeon subsystems
 */
class FLingotionThespeonSubsystemUtils
{
  public:
	/**
	 * Gets a game instance subsystem of the specified type
	 * @return The subsystem instance, or nullptr if not found
	 */
	template <typename T> static T* GetGameInstanceSubsystem()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.OwningGameInstance)
			{
				return Context.OwningGameInstance->GetSubsystem<T>();
			}
		}

		return nullptr;
	}

	/**
	 * Gets the InferenceWorkloadManager subsystem from the current game instance
	 * @return The subsystem instance, or nullptr if not found
	 */
	static UInferenceWorkloadManager* GetInferenceWorkloadManager();

	/**
	 * Gets the LookupTableManager subsystem from the current game instance
	 * @return The subsystem instance, or nullptr if not found
	 */
	static ULookupTableManager* GetLookupTableManager();

	/**
	 * Gets the ModuleManager subsystem from the current game instance
	 * @return The subsystem instance, or nullptr if not found
	 */
	static UModuleManager* GetModuleManager();

	/**
	 * Gets both subsystems in a single call
	 * @param OutInferenceWorkloadManager The InferenceWorkloadManager subsystem
	 * @param OutLookupTableManager The LookupTableManager subsystem
	 * @return True if both subsystems were found
	 */
	static bool GetSubsystems(UInferenceWorkloadManager*& OutInferenceWorkloadManager, ULookupTableManager*& OutLookupTableManager);
};