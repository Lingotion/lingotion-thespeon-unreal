// Copyright 2025 - 2026 Lingotion AB All Rights Reserved

#include "Engine/ThespeonComponent.h"
#include "Inference/ThespeonInference.h"
#include "Inference/InferenceSession.h"
#include "Inference/PreloadSession.h"
#include "Inference/InferenceWorkloadManager.h"
#include "Inference/ModuleManager.h"
#include "Language/LookupTableManager.h"
#include "Core/ManifestHandler.h"
#include "Utils/SubsystemUtils.h"
#include "Serialization/BufferArchive.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

// Custom destructors for forward-declared types (TUniquePtr requires complete type at destruction)
void FThespeonInferenceDeleter::operator()(Thespeon::Inference::ThespeonInference* Ptr) const
{
	delete Ptr;
}

void FPreloadSessionDeleter::operator()(Thespeon::Inference::FPreloadSession* Ptr) const
{
	delete Ptr;
}

UThespeonComponent::UThespeonComponent() : Super()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Destructor must be defined in .cpp file where ThespeonInference is a complete type
// This allows TUniquePtr to properly delete the forward-declared type
UThespeonComponent::~UThespeonComponent() = default;

void UThespeonComponent::OnRegister()
{
	Super::OnRegister();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	USceneComponent* Root = Owner->GetRootComponent();
	if (!Root)
	{
		// Create a default root if none exists
		Root = NewObject<USceneComponent>(Owner, TEXT("DefaultSceneRoot"));
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
	}
}

void UThespeonComponent::CleanupThread()
{
	if (SessionThread)
	{
		if (Session)
		{
			// Request stop with component destruction reason
			Session->StopWithReason(Thespeon::Inference::ESessionStopReason::ComponentDestroyed);
		}

		SessionThread->WaitForCompletion();

		SessionThread.Reset();
	}

	if (Session)
	{
		Session = nullptr;
	}

	bIsSynthesizing.store(false);
}

void UThespeonComponent::CleanupPreloadThreads()
{
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Cleaning up preload threads %d."), PreloadThreads.Num());
	for (int32 i = 0; i < PreloadThreads.Num(); ++i)
	{
		if (!PreloadThreads[i])
		{
			continue;
		}
		if (ActivePreloadSessions.IsValidIndex(i) && ActivePreloadSessions[i])
		{
			ActivePreloadSessions[i]->Stop();
		}
		const double StartTime = FPlatformTime::Seconds();
		PreloadThreads[i]->WaitForCompletion();
		const double Elapsed = FPlatformTime::Seconds() - StartTime;

		// During PIE teardown, the game thread may be blocked here in WaitForCompletion()
		// while TaskGraph tasks inside FPreloadSession try to dispatch LoadModelsAsync() back
		// to the game thread. LoadModelsAsync's internal 10-second timeout breaks this stall.
		// A shutdown exceeding this threshold suggests that timeout was hit.
		constexpr double SlowShutdownThresholdSeconds = 2.0;
		if (Elapsed > SlowShutdownThresholdSeconds)
		{
			LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Preload thread took %.1fs to shut down."), Elapsed);
		}
		PreloadThreads[i].Reset();
	}
	PreloadThreads.Empty();
	ActivePreloadSessions.Empty();
	ActivePreloadCount.store(0);
	PreloadGroupPendingCounts.Empty();
	PreloadGroupHadFailure.Empty();
}

void UThespeonComponent::CancelSynthesis()
{
	if (!bIsSynthesizing.load() || !Session)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("called but no synthesis is running"));
		return;
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Requesting stop for current session"));
	Session->StopWithReason(Thespeon::Inference::ESessionStopReason::UserCancelled);
	SessionThread->WaitForCompletion();
	bIsSynthesizing.store(false);
	ProcessPendingRequests();
}

void UThespeonComponent::ResetState()
{
	CleanupThread();
	CleanupPreloadThreads(); // also resets ActivePreloadCount
	SynthRequestQueue.Empty();
	PreloadRequests.Empty();
}

// Called when component is unregistered - might be called without actually GC:ing the actor component
void UThespeonComponent::OnUnregister()
{
	Super::OnUnregister();
	ResetState();
}

// Called when actor component is GC:ed
void UThespeonComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ResetState();
}

/*
 * Currently only Windows supports GPU via DML, on other platforms we fallback to CPU
 *
 * @param EBackendType Requested The requested backend type
 * @return EBackendType The resolved backend type
 */
static EBackendType ResolveBackend(EBackendType Requested)
{
#if !PLATFORM_WINDOWS
	if (Requested == EBackendType::GPU)
	{
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("GPU backend not supported on this platform. Falling back to CPU."));
		return EBackendType::CPU;
	}
#endif
	return Requested;
}

/**
 * Returns the default backend type when BackendType is None.
 *
 * @param EBackendType Requested The requested backend type
 * @return EBackendType The resolved default backend type
 */
static EBackendType GetDefaultBackendType(EBackendType Requested)
{
	if (Requested != EBackendType::None)
	{
		return Requested;
	}

	EBackendType DefaultBackend = FInferenceConfig().BackendType;
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug, TEXT("No backend type specified in inference config, using default: %s"), *UEnum::GetValueAsString(DefaultBackend)
	);
	return DefaultBackend;
}

void UThespeonComponent::PruneFinishedPreloadThreads()
{
	for (int32 i = PreloadThreads.Num() - 1; i >= 0; --i)
	{
		if (!PreloadThreads[i])
		{
			continue;
		}
		if (ActivePreloadSessions.IsValidIndex(i) && ActivePreloadSessions[i] && ActivePreloadSessions[i]->IsFinished())
		{
			PreloadThreads[i]->WaitForCompletion();
			LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Pruning finished preload thread %d."), i);
			PreloadThreads.RemoveAt(i);
			ActivePreloadSessions.RemoveAt(i);
		}
	}
}

void UThespeonComponent::ProcessPendingRequests()
{
	PruneFinishedPreloadThreads();
	if (bIsSynthesizing.load())
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("ProcessPendingRequests called but synthesis is still in progress. Waiting for completion before popping next request.")
		);
		return;
	}
	// Synthesis must wait for all active preloads to finish before starting,
	// since a preload may be loading the character needed for the next synth.
	if (!SynthRequestQueue.IsEmpty() && ActivePreloadCount.load() > 0)
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug, TEXT("ProcessPendingRequests: synthesis request pending but preloads are still in progress. Waiting.")
		);
		return;
	}
	SynthRequest PoppedRequest;
	if (SynthRequestQueue.Dequeue(PoppedRequest))
	{
		PreloadRequest CorrespondingPreload{
		    PoppedRequest.Input.CharacterName, PoppedRequest.Input.ModuleType, PoppedRequest.InferenceConfig.BackendType
		};
		if (PreloadRequests.Contains(CorrespondingPreload))
		{
			PreloadRequests.Remove(CorrespondingPreload); // remove explicit preload request and run it in RunSynth instead.
		}
		LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Dequeued synthesis request for session '%s'. Starting synthesis."), *PoppedRequest.SessionId);
		RunSynthesisRequest(PoppedRequest);
		return;
	}

	// Launch all queued preloads in parallel.
	while (!PreloadRequests.IsEmpty())
	{
		if (ActivePreloadCount.load() >= MaxConcurrentPreloads)
		{
			return;
		}

		LINGO_LOG_FUNC(
		    EVerbosityLevel::Info,
		    TEXT("No pending synthesis requests. Dequeuing preload request for character '%s', module '%s', backend '%s'. Starting preload."),
		    *PreloadRequests[0].CharacterName,
		    *UEnum::GetValueAsString(PreloadRequests[0].ModuleType),
		    *UEnum::GetValueAsString(PreloadRequests[0].BackendType)
		);
		PreloadRequest TopPreload = PreloadRequests[0];
		PreloadRequests.RemoveAt(0);
		RunPreloadRequest(TopPreload);
	}
}

// Main synthesis entry point. Schedules a synth and then tries to start it if no other synth or preload is running.
void UThespeonComponent::Synthesize(FLingotionModelInput Input, FString SessionId, FInferenceConfig InferenceConfig)
{
	SynthRequestQueue.Enqueue({Input, SessionId, InferenceConfig});
	LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Enqueued synthesis request for session '%s'."), *SessionId);
	ProcessPendingRequests();
}

// Runs synthesis. Validates input, starts audio streaming, and spawns a background inference thread.
void UThespeonComponent::RunSynthesisRequest(SynthRequest Request)
{
	FLingotionModelInput Input = Request.Input;
	FString SessionId = Request.SessionId;
	FInferenceConfig InferenceConfig = Request.InferenceConfig;
	if (bIsSynthesizing.load())
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("RunSynthesisRequest called while already synthesizing.")
		); // This should never happen because ProcessPendingRequests checks
		   // for this, but we check again here for extra safety.
		return;
	}
	if (Input.Segments.Num() == 0)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Synthesize called with empty input segments. Ignoring this call. Make sure to provide at least one segment with text to synthesize."
		    )
		);
		return;
	}
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("ThespeonComponent Synthesize called with config: %s"), *InferenceConfig.ToString());

	// Log the input data for debugging
	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Synthesize called with input: %s"), *Input.ToJson());

	InferenceConfig.BackendType = ResolveBackend(GetDefaultBackendType(InferenceConfig.BackendType));
	if (!Input.ValidateCharacterModule(InferenceConfig.ModuleType))
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("Invalid character module specified (%s, %s) and fallback attempts failed. Aborting synthesis. Please check your imported modules."),
		    *Input.CharacterName,
		    *UEnum::GetValueAsString(Input.ModuleType)
		);
		return; // mayhaps perchance do we want to make Synthesize return a bool indicating success/failure instead of just early returning without a
		        // signal?
	}

	TWeakObjectPtr<UThespeonComponent> WeakThis(this);

	// Run inference on a background thread to avoid blocking the game thread

	// Capture subsystem pointers on game thread — the ::Get() methods access UObject internals
	// (GEngine, UGameInstance::GetSubsystem<>) which must not be called from worker threads.
	// See ULookupTableManager::Get() check(IsInGameThread()) and UE SubsystemCollection.cpp:158.
	UInferenceWorkloadManager* WorkloadMgr = FLingotionThespeonSubsystemUtils::GetInferenceWorkloadManager();
	UModuleManager* ModuleMgr = UModuleManager::Get();
	ULookupTableManager* LookupMgr = ULookupTableManager::Get();
	UManifestHandler* ManifestMgr = UManifestHandler::Get();

	// extra safety check to make sure that no previous session is left running
	CleanupThread();
	Session = TUniquePtr<Thespeon::Inference::ThespeonInference, FThespeonInferenceDeleter>(
	    new Thespeon::Inference::ThespeonInference(Input, InferenceConfig, SessionId, WorkloadMgr, ModuleMgr, LookupMgr, ManifestMgr)
	);
	Session->OnDataSynthesized.BindUObject(WeakThis.Get(), &UThespeonComponent::PacketHandler);

	// creates and calls the FRunnable functions in the following order:
	// Init()->Run()->Stop()->Exit().
	SessionThread.Reset(
	    FRunnableThread::Create(Session.Get(), TEXT("ThespeonBenchmarkThread"), 0, ConvertToNativeThreadPriority(InferenceConfig.ThreadPriority))
	);

	bIsSynthesizing.store(true);
}

// Launches preloading on a background thread and broadcasts OnPreloadComplete on the game thread when done.
void UThespeonComponent::PreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig)
{
	PreloadRequest NewRequest{CharacterName, ModuleType, InferenceConfig.BackendType};
	if (PreloadRequests.Contains(NewRequest))
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug, TEXT("PreloadCharacter called with a request that is already in the queue. Ignoring duplicate request.")
		);
		return;
	}
	PreloadRequests.Add(NewRequest);
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Info,
	    TEXT("Enqueued preload request for character '%s', module '%s', backend '%s'."),
	    *CharacterName,
	    *UEnum::GetValueAsString(ModuleType),
	    *UEnum::GetValueAsString(InferenceConfig.BackendType)
	);
	ProcessPendingRequests();
}

void UThespeonComponent::PreloadCharacterGroup(TArray<FPreloadEntry> Characters, FString PreloadGroupId)
{
	// Collect valid (non-duplicate) requests first so the count is known before any thread starts.
	// This prevents OnPreloadGroupComplete from firing prematurely if an early preload completes
	// before the remaining PreloadCharacter calls have had a chance to increment the counter.
	TArray<PreloadRequest> NewRequests;
	for (const FPreloadEntry& Entry : Characters)
	{
		PreloadRequest Req{Entry.CharacterName, Entry.ModuleType, Entry.InferenceConfig.BackendType, PreloadGroupId};
		if (PreloadRequests.Contains(Req) || NewRequests.Contains(Req))
		{
			LINGO_LOG_FUNC(
			    EVerbosityLevel::Debug, TEXT("PreloadCharacterGroup: duplicate request for character '%s', ignoring."), *Entry.CharacterName
			);
			continue;
		}
		NewRequests.Add(Req);
	}

	if (NewRequests.IsEmpty())
	{
		return;
	}

	// Register all counts atomically before launching any thread.
	if (!PreloadGroupId.IsEmpty())
	{
		PreloadGroupPendingCounts.FindOrAdd(PreloadGroupId, 0) += NewRequests.Num();
		PreloadGroupHadFailure.FindOrAdd(PreloadGroupId, false);
	}

	for (PreloadRequest& Req : NewRequests)
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Info,
		    TEXT("Enqueued preload request for character '%s', module '%s', backend '%s', group '%s'."),
		    *Req.CharacterName,
		    *UEnum::GetValueAsString(Req.ModuleType),
		    *UEnum::GetValueAsString(Req.BackendType),
		    *PreloadGroupId
		);
		PreloadRequests.Add(Req);
	}

	ProcessPendingRequests();
}

void UThespeonComponent::RunPreloadRequest(PreloadRequest Request)
{
	FString CharacterName = Request.CharacterName;
	EThespeonModuleType ModuleType = Request.ModuleType;
	EBackendType BackendType = ResolveBackend(GetDefaultBackendType(Request.BackendType));
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug,
	    TEXT("Preloading CharacterName: %s, ModuleType: %s, resolved BackendType: %s"),
	    *CharacterName,
	    *UEnum::GetValueAsString(ModuleType),
	    *UEnum::GetValueAsString(BackendType)
	);

	// Capture subsystem pointers on game thread — the ::Get() methods access UObject internals
	// (GEngine, UGameInstance::GetSubsystem<>) which must not be called from worker threads.
	// See ULookupTableManager::Get() check(IsInGameThread()) and UE SubsystemCollection.cpp:158.
	UInferenceWorkloadManager* WorkloadMgr = FLingotionThespeonSubsystemUtils::GetInferenceWorkloadManager();
	UModuleManager* ModuleMgr = UModuleManager::Get();
	ULookupTableManager* LookupMgr = ULookupTableManager::Get();
	UManifestHandler* ManifestMgr = UManifestHandler::Get();

	// Set up completion callback with TWeakObjectPtr guard for lifetime safety.
	// The callback is captured by value in the game-thread lambda inside FPreloadSession,
	// so it does not reference FPreloadSession members after Run() returns.
	TWeakObjectPtr<UThespeonComponent> WeakThis(this);
	Thespeon::Inference::FPreloadSession::FOnComplete OnComplete =
	    [WeakThis, PreloadGroupId = Request.PreloadGroupId](bool bSuccess, FString Name, EThespeonModuleType Type, EBackendType Backend)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnPreloadComplete.Broadcast(bSuccess, Name, Type, Backend);
			WeakThis->ActivePreloadCount.fetch_sub(1);

			if (!PreloadGroupId.IsEmpty())
			{
				if (!bSuccess)
				{
					WeakThis->PreloadGroupHadFailure[PreloadGroupId] = true;
				}
				int32& Count = WeakThis->PreloadGroupPendingCounts[PreloadGroupId];
				if (--Count == 0)
				{
					const bool bAllSucceeded = !WeakThis->PreloadGroupHadFailure[PreloadGroupId];
					WeakThis->PreloadGroupPendingCounts.Remove(PreloadGroupId);
					WeakThis->PreloadGroupHadFailure.Remove(PreloadGroupId);
					WeakThis->OnPreloadGroupComplete.Broadcast(PreloadGroupId, bAllSucceeded);
				}
			}

			WeakThis->ProcessPendingRequests();
		}
	};

	ActivePreloadCount.fetch_add(1);
	ActivePreloadSessions.Add(TUniquePtr<Thespeon::Inference::FPreloadSession, FPreloadSessionDeleter>(new Thespeon::Inference::FPreloadSession(
	    CharacterName, ModuleType, BackendType, WorkloadMgr, ModuleMgr, LookupMgr, ManifestMgr, MoveTemp(OnComplete)
	)));
	PreloadThreads.Emplace(FRunnableThread::Create(ActivePreloadSessions.Last().Get(), TEXT("ThespeonPreloadThread"), 0, TPri_Normal));
}

bool UThespeonComponent::TryUnloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, EBackendType BackendType)
{
	BackendType = ResolveBackend(BackendType); // resolve directly to a correct type.
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Info,
	    TEXT("ThespeonComponent TryUnloadCharacter called with CharacterName: %s, ModuleType: %s"),
	    *CharacterName,
	    *UEnum::GetValueAsString(ModuleType)
	);
	// Capture subsystem pointers on game thread — the ::Get() methods access UObject internals
	// (GEngine, UGameInstance::GetSubsystem<>) which must not be called from worker threads.
	UInferenceWorkloadManager* WorkloadMgr = FLingotionThespeonSubsystemUtils::GetInferenceWorkloadManager();
	UModuleManager* ModuleMgr = UModuleManager::Get();
	ULookupTableManager* LookupMgr = ULookupTableManager::Get();
	UManifestHandler* ManifestMgr = UManifestHandler::Get();

	// if backend type is None all preloaded data will be unloaded
	return Thespeon::Inference::ThespeonInference::TryUnloadCharacter(
	    CharacterName, ModuleType, BackendType, WorkloadMgr, ModuleMgr, LookupMgr, ManifestMgr
	);
}

// Dispatched on the game thread by the inference session whenever data is ready.
// Routes packets by type: audio data is buffered until BufferSeconds is met, then flushed
// to either the OnAudioReceived delegate or the AudioStreamComponent for playback.
void UThespeonComponent::PacketHandler(const FString& SessionID, const Thespeon::Core::FThespeonDataPacket& Packet)
{
	switch (Packet.CallbackType)
	{
		case Thespeon::Core::SynthCallbackType::CB_AUDIO:
		{
			if (!Packet.Payload.IsType<TArray<float>>())
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Returned audio packet was malformed, ignoring."));
				return;
			}
			// check metadata for "is_final"
			bool bIsFinalPacket = false;
			const Thespeon::Core::FPacketMetadataValue* MetadataVal = Packet.Metadata.Find(TEXT("is_final"));
			if (MetadataVal && MetadataVal->IsType<bool>())
			{
				bIsFinalPacket = MetadataVal->Get<bool>();
			}

			const TArray<float>& Samples = Packet.Payload.Get<TArray<float>>();

			AudioDataQueue.Enqueue(Samples);
			CurrentDataLength.fetch_add(Samples.Num());
			if (bIsFinalPacket || CurrentDataLength.load() >= FInferenceConfig().BufferSeconds * 44100)
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Releasing queued audio packets, total samples: %d"), CurrentDataLength.load());
				TArray<float> P;
				while (AudioDataQueue.Dequeue(P))
				{
					// call bound callback instead of streaming if possible
					if (SessionID == TEXT("LINGOTION_WARMUP"))
					{
						continue;
					}
					if (OnAudioReceived.IsBound())
					{
						OnAudioReceived.Broadcast(SessionID, P);
					}
				}
				if (bIsFinalPacket)
				{
					bIsSynthesizing.store(false);
					CurrentDataLength.store(0);
					if (OnSynthesisComplete.IsBound())
					{
						OnSynthesisComplete.Broadcast(SessionID);
					}
					ProcessPendingRequests();
				}
			}
			return;
		}

		case Thespeon::Core::SynthCallbackType::CB_TRIGGERSAMPLE:
		{
			if (!Packet.Payload.IsType<TArray<int64>>())
			{
				LINGO_LOG(EVerbosityLevel::Error, TEXT("Returned sample packet was malformed, ignoring."));
				return;
			}

			const TArray<int64>& Samples = Packet.Payload.Get<TArray<int64>>();

			// If sample trigger callback is bound, call it.
			if (OnAudioSampleRequestReceived.IsBound())
			{
				OnAudioSampleRequestReceived.Broadcast(SessionID, Samples);
			}
			return;
		}

		case Thespeon::Core::SynthCallbackType::CB_ERROR:
		{
			bIsSynthesizing.store(false);
			CurrentDataLength.store(0);
			bool bWasCancelled = false;
			const Thespeon::Core::FPacketMetadataValue* MetadataVal = Packet.Metadata.Find(TEXT("was_cancelled"));
			if (MetadataVal && MetadataVal->IsType<bool>())
			{
				bWasCancelled = MetadataVal->Get<bool>();
			}
			// Distinguish between cancellation and error
			if (bWasCancelled)
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Info, TEXT("Synthesis cancelled for session: %s"), *SessionID);
				AudioDataQueue.Empty();
				// Cancellation is user-initiated, no delegate needed - user already knows they cancelled
				ProcessPendingRequests();
				return;
			}

			LINGO_LOG(EVerbosityLevel::Error, TEXT("Received packet with error status, dropping"));
			if (OnSynthesisFailed.IsBound())
			{
				OnSynthesisFailed.Broadcast(SessionID);
			}
			AudioDataQueue.Empty();
			ProcessPendingRequests();
			return;
		}

		case Thespeon::Core::SynthCallbackType::CB_UNSPECIFIED:
		default:
		{
			LINGO_LOG(EVerbosityLevel::Warning, TEXT("Unknown packet type received"));
		}
	}
}

EThreadPriority UThespeonComponent::ConvertToNativeThreadPriority(EThreadPriorityWrapper Wrapper) const
{
	switch (Wrapper)
	{
		case EThreadPriorityWrapper::Normal:
			return TPri_Normal;
		case EThreadPriorityWrapper::AboveNormal:
			return TPri_AboveNormal;
		case EThreadPriorityWrapper::BelowNormal:
			return TPri_BelowNormal;
		case EThreadPriorityWrapper::Highest:
			return TPri_Highest;
		case EThreadPriorityWrapper::Lowest:
			return TPri_Lowest;
		case EThreadPriorityWrapper::SlightlyBelowNormal:
			return TPri_SlightlyBelowNormal;
		case EThreadPriorityWrapper::TimeCritical:
			return TPri_TimeCritical;
		default:
			return TPri_AboveNormal;
	}
}
