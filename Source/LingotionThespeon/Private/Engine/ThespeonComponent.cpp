// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Engine/ThespeonComponent.h"
#include "Inference/ThespeonInference.h"
#include "Inference/InferenceSession.h"
#include "Serialization/BufferArchive.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"

// Custom destructor for forward-declared ThespeonInference
void FThespeonInferenceDeleter::operator()(Thespeon::Inference::ThespeonInference* Ptr) const
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

		delete SessionThread;
		SessionThread = nullptr;
	}

	if (Session)
	{
		Session = nullptr;
	}

	bIsSynthesizing = false;
}

void UThespeonComponent::CancelSynthesis()
{
	if (!bIsSynthesizing || !Session)
	{
		LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("called but no synthesis is running"));
		return;
	}

	LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Requesting stop for current session"));
	Session->StopWithReason(Thespeon::Inference::ESessionStopReason::UserCancelled);
	SessionThread->WaitForCompletion();
	bIsSynthesizing = false;
	ProcessPendingRequests();
}

// Called when component is unregistered - might be called without actually GC:ing the actor component
void UThespeonComponent::OnUnregister()
{

	Super::OnUnregister();
	CleanupThread();
	bIsPreloading = false;
	SynthRequestQueue.Empty();
	PreloadRequests.Empty();
}

// Called when actor component is GC:ed
void UThespeonComponent::BeginDestroy()
{
	Super::BeginDestroy();
	CleanupThread();
	bIsPreloading = false;
	SynthRequestQueue.Empty();
	PreloadRequests.Empty();
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
		LINGO_LOG(EVerbosityLevel::Warning, TEXT("GPU backend not supported on Windows platform, falling back to CPU"));
		return EBackendType::CPU;
	}
#endif
	return Requested;
}

void UThespeonComponent::ProcessPendingRequests()
{
	if (bIsSynthesizing || bIsPreloading)
	{
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("ProcessPendingRequests called but synthesis or preloading is still in progress. Waiting for completion before popping next request."
		    )
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
	if (!PreloadRequests.IsEmpty())
	{
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
	if (bIsSynthesizing)
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

	if (InferenceConfig.BackendType == EBackendType::None) // None interpreted as default in synthesis
	{
		FInferenceConfig DefaultConfig = FInferenceConfig();
		InferenceConfig.BackendType = DefaultConfig.BackendType;
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug,
		    TEXT("No backend type specified in inference config, using default: %s"),
		    *UEnum::GetValueAsString(InferenceConfig.BackendType)
		);
	}
	InferenceConfig.BackendType = ResolveBackend(InferenceConfig.BackendType); // resolve directly to a correct type.
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

	// extra safety check to make sure that no previous session is left running
	CleanupThread();
	Session = TUniquePtr<Thespeon::Inference::ThespeonInference, FThespeonInferenceDeleter>(
	    new Thespeon::Inference::ThespeonInference(Input, InferenceConfig, SessionId)
	);
	Session->OnDataSynthesized.BindUObject(WeakThis.Get(), &UThespeonComponent::PacketHandler);

	// creates and calls the FRunnable functions in the following order:
	// Init()->Run()->Stop()->Exit().
	SessionThread =
	    FRunnableThread::Create(Session.Get(), TEXT("ThespeonBenchmarkThread"), 0, ConvertToNativeThreadPriority(InferenceConfig.ThreadPriority));

	bIsSynthesizing = true;
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

void UThespeonComponent::RunPreloadRequest(PreloadRequest Request)
{
	if (bIsPreloading)
	{
		LINGO_LOG(
		    EVerbosityLevel::Error,
		    TEXT("RunPreloadRequest called but a preload is already in progress.")
		); // This should never happen because ProcessPendingRequests checks
		   // for this, but we check again here for extra safety.
		return;
	}
	FString CharacterName = Request.CharacterName;
	EThespeonModuleType ModuleType = Request.ModuleType;
	EBackendType BackendType = Request.BackendType;
	if (BackendType == EBackendType::None) // None interpreted as default in Preload
	{
		FInferenceConfig DefaultConfig = FInferenceConfig();
		BackendType = DefaultConfig.BackendType;
		LINGO_LOG_FUNC(
		    EVerbosityLevel::Debug, TEXT("No backend type specified in inference config, using default: %s"), *UEnum::GetValueAsString(BackendType)
		);
	}
	BackendType = ResolveBackend(BackendType); // resolve directly to a correct type.
	LINGO_LOG_FUNC(
	    EVerbosityLevel::Debug,
	    TEXT("Preloading CharacterName: %s, ModuleType: %s, resolved BackendType: %s"),
	    *CharacterName,
	    *UEnum::GetValueAsString(ModuleType),
	    *UEnum::GetValueAsString(BackendType)
	);
	// Some other threading solution should be put here when doing TUNR-134
	TWeakObjectPtr<UThespeonComponent> WeakThis(this);
	AsyncTask(
	    ENamedThreads::AnyBackgroundThreadNormalTask,
	    [CharacterName, ModuleType, BackendType, WeakThis]()
	    {
		    bool bSuccess = Thespeon::Inference::ThespeonInference::TryPreloadCharacter(CharacterName, ModuleType, BackendType);
		    if (!WeakThis.IsValid())
		    {
			    return;
		    }
		    AsyncTask(
		        ENamedThreads::GameThread,
		        [WeakThis, bSuccess, CharacterName, ModuleType, BackendType]()
		        {
			        if (WeakThis.IsValid())
			        {
				        if (WeakThis->OnPreloadComplete.IsBound())
				        {
					        WeakThis->OnPreloadComplete.Broadcast(bSuccess, CharacterName, ModuleType, BackendType);
				        }
				        WeakThis->bIsPreloading = false;
				        WeakThis->ProcessPendingRequests();
			        }
		        }
		    );
	    }
	);
	bIsPreloading = true;
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
	// if backend type is None all preloaded data will be unloaded
	return Thespeon::Inference::ThespeonInference::TryUnloadCharacter(CharacterName, ModuleType, BackendType);
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
			CurrentDataLength += Samples.Num();
			if (bIsFinalPacket || CurrentDataLength >= FInferenceConfig().BufferSeconds * 44100)
			{
				LINGO_LOG_FUNC(EVerbosityLevel::Debug, TEXT("Releasing queued audio packets, total samples: %d"), CurrentDataLength);
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
					bIsSynthesizing = false;
					CurrentDataLength = 0;
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
			bIsSynthesizing = false;
			CurrentDataLength = 0;
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
