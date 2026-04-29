// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "PreloadSession.h"
#include "InferenceWorkloadManager.h"
#include "ModuleManager.h"
#include "Language/LookupTableManager.h"
#include "Core/ManifestHandler.h"
#include "Character/CharacterModule.h"
#include "Language/LanguageModule.h"
#include "Core/LingotionLogger.h"
#include "Async/Async.h"

namespace Thespeon
{
namespace Inference
{

FPreloadSession::FPreloadSession(
    FString InCharacterName,
    EThespeonModuleType InModuleType,
    EBackendType InBackendType,
    UInferenceWorkloadManager* InWorkloadMgr,
    UModuleManager* InModuleMgr,
    ULookupTableManager* InLookupMgr,
    UManifestHandler* InManifest,
    FOnComplete InOnComplete
)
    : CharacterName(MoveTemp(InCharacterName))
    , ModuleType(InModuleType)
    , BackendType(InBackendType)
    , WorkloadMgr(InWorkloadMgr)
    , ModuleMgr(InModuleMgr)
    , LookupMgr(InLookupMgr)
    , ManifestHandler(InManifest)
    , OnComplete(MoveTemp(InOnComplete))
{
}

uint32 FPreloadSession::Run()
{
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Starting preload for '%s' ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));

	// --- Validate subsystem pointers (captured on game thread) ---
	if (!ManifestHandler || !ModuleMgr || !WorkloadMgr || !LookupMgr)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("One or more subsystem pointers are null"));
		FireOnComplete(false);
		return 0;
	}

	if (ShouldStop())
	{
		FireOnComplete(false);
		return 0;
	}

	// --- Get character module entry from manifest ---
	Thespeon::Core::FModuleEntry Entry = ManifestHandler->GetCharacterModuleEntry(CharacterName, ModuleType);
	if (Entry.ModuleID.IsEmpty())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Character '%s' of module type '%s' has not been imported and cannot be preloaded. "
		         "Check the Lingotion Thespeon Info window (Window > Lingotion Thespeon Info) to see your imported character modules."),
		    *CharacterName,
		    *UEnum::GetValueAsString(ModuleType)
		);
		FireOnComplete(false);
		return 0;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Got Character Entry: %s, %s, %s"), *Entry.ModuleID, *Entry.JsonPath, *Entry.Version.ToString());

	// --- Get CharacterModule from ModuleManager ---
	// TSharedPtr keeps module alive for this scope; raw pointer used for downstream API.
	TSharedPtr<Thespeon::Character::CharacterModule, ESPMode::ThreadSafe> CharacterModulePtr =
	    ModuleMgr->GetModule<Thespeon::Character::CharacterModule>(Entry);
	if (!CharacterModulePtr)
	{
		LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get CharacterModule"));
		FireOnComplete(false);
		return 0;
	}
	Thespeon::Character::CharacterModule* CharacterModule = CharacterModulePtr.Get();
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Got Character Module: %s"), *CharacterModule->ModuleID);

	if (ShouldStop())
	{
		FireOnComplete(false);
		return 0;
	}

	// --- Collect language module entries ---
	// Language codes are unique (source is a TMap). Only the entry is needed downstream
	// (lang code is not used by the parallel tasks), so a flat array suffices.
	TArray<Thespeon::Core::FModuleEntry> LangEntries;
	for (const auto& Kvp : CharacterModule->LanguageModuleIDs)
	{
		Thespeon::Core::FModuleEntry LangEntry = ManifestHandler->GetLanguageModuleEntry(Kvp.Value);
		if (LangEntry.ModuleID.IsEmpty())
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("No language module entry found for ModuleName: %s"), *Kvp.Value);
			continue;
		}
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug, TEXT("Got Language Entry: %s, %s, %s"), *LangEntry.ModuleID, *LangEntry.JsonPath, *LangEntry.Version.ToString()
		);
		LangEntries.Add(MoveTemp(LangEntry));
	}
	if (LangEntries.Num() == 0)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error, TEXT("None of the supported languages for character module was imported. Make sure to import at least one.")
		);
		FireOnComplete(false);
		return 0;
	}

	if (ShouldStop())
	{
		FireOnComplete(false);
		return 0;
	}

	// --- Dispatch per-module loads as parallel TaskGraph tasks ---
	// Each task captures subsystem raw pointers by value (not `this`) plus a pointer to
	// the stop flag. This is safe because:
	//   1. Subsystem pointers outlive the preload — OnUnregister (where we join) precedes
	//      GameInstance::Shutdown (where subsystems deinitialize).
	//   2. The stop flag pointer is valid because Run() always waits for all futures before
	//      returning, keeping FPreloadSession alive for the duration of every task.
	TArray<TFuture<bool>> Futures;
	std::atomic<bool>* StopFlag = &bStopRequested;

	// Language module tasks
	for (const Thespeon::Core::FModuleEntry& LangEntry : LangEntries)
	{
		Futures.Add(Async(
		    EAsyncExecution::TaskGraph,
		    [StopFlag,
		     ModuleMgr = this->ModuleMgr,
		     WorkloadMgr = this->WorkloadMgr,
		     LookupMgr = this->LookupMgr,
		     LangBackendType = this->BackendType,
		     LangEntry]() -> bool
		    {
			    if (StopFlag->load())
			    {
				    return false;
			    }

			    TSharedPtr<Thespeon::Language::LanguageModule, ESPMode::ThreadSafe> LangModulePtr =
			        ModuleMgr->GetModule<Thespeon::Language::LanguageModule>(LangEntry);
			    if (!LangModulePtr)
			    {
				    LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to get LanguageModule"));
				    return false;
			    }
			    Thespeon::Language::LanguageModule* LangModule = LangModulePtr.Get();

			    if (StopFlag->load())
			    {
				    return false;
			    }

			    LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Registering workload for Language Module: %s"), *(LangModule->ModuleID));
			    if (!WorkloadMgr->RegisterModuleWorkload(LangModule, LangBackendType))
			    {
				    LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to register LanguageModule workload: %s"), *(LangModule->ModuleID));
				    return false;
			    }

			    if (StopFlag->load())
			    {
				    return false;
			    }

			    LookupMgr->RegisterLookupTable(LangModule);
			    LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Successfully registered lookup table for language module: %s"), *(LangModule->ModuleID));
			    return true;
		    }
		));
	}

	// Character module task — independent of language module tasks.
	// Both register workloads via the thread-safe InferenceWorkloadManager.
	Futures.Add(Async(
	    EAsyncExecution::TaskGraph,
	    [StopFlag, WorkloadMgr = this->WorkloadMgr, CharBackendType = this->BackendType, CharacterModule]() -> bool
	    {
		    if (StopFlag->load())
		    {
			    return false;
		    }

		    if (!WorkloadMgr->RegisterModuleWorkload(CharacterModule, CharBackendType))
		    {
			    LINGO_LOG(EVerbosityLevel::Error, TEXT("Failed to register CharacterModule workload: %s"), *(CharacterModule->ModuleID));
			    return false;
		    }
		    return true;
	    }
	));

	// --- Wait for all futures ---
	// We MUST wait for all futures even during cancellation. The task lambdas hold raw pointers
	// to subsystems that are valid only as long as Run() hasn't returned (because CleanupPreloadThread
	// joins this thread in OnUnregister, which precedes subsystem Deinitialize).
	// Potential deadlock during teardown: tasks call LoadModelsAsync() which dispatches to game thread
	// and waits on TFuture. If game thread is blocked in WaitForCompletion(), LoadModelsAsync's
	// 10-second timeout resolves the deadlock. The StopFlag checks in tasks provide early exit
	// before reaching LoadModelsAsync.
	bool bAllSucceeded = true;
	bool bLoggedCancel = false;
	for (TFuture<bool>& Future : Futures)
	{
		while (!Future.WaitFor(FTimespan::FromMilliseconds(100)))
		{
			// Periodic check — cannot cancel already-dispatched TaskGraph tasks,
			// but logging helps diagnose slow shutdowns. One-shot to avoid spam
			// during the potential 10s LoadModelsAsync timeout.
			if (ShouldStop() && !bLoggedCancel)
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Preload cancelled, waiting for outstanding tasks to finish..."));
				bLoggedCancel = true;
			}
		}
		if (!Future.Get())
		{
			bAllSucceeded = false;
		}
	}

	const bool bFinalSuccess = bAllSucceeded && !ShouldStop();
	bSuccess.store(bFinalSuccess);

	LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Preload for '%s' %s."), *CharacterName, bFinalSuccess ? TEXT("succeeded") : TEXT("failed"));

	FireOnComplete(bFinalSuccess);
	return bFinalSuccess ? 1 : 0;
}

void FPreloadSession::Stop()
{
	bStopRequested.store(true);
}

void FPreloadSession::FireOnComplete(bool bResult)
{
	// Capture everything by value — the game-thread lambda must not reference FPreloadSession
	// members, because the owning thread may join and destroy this object before the lambda runs.
	FString Name = CharacterName;
	EThespeonModuleType Type = ModuleType;
	EBackendType Backend = BackendType;
	FOnComplete Callback = MoveTemp(OnComplete);

	AsyncTask(
	    ENamedThreads::GameThread,
	    [Callback = MoveTemp(Callback), bResult, Name = MoveTemp(Name), Type, Backend]()
	    {
		    if (Callback)
		    {
			    Callback(bResult, Name, Type, Backend);
		    }
	    }
	);
}

} // namespace Inference
} // namespace Thespeon
