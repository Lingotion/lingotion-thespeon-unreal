// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 558341-4138, Sweden. The code and software may only be used and distributed according to the Terms of Service and Use found at www.lingotion.com.

#include "Engine/ThespeonComponent.h"
#include "Inference/ThespeonInference.h"
#include "Serialization/BufferArchive.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/AudioStreamComponent.h" 
#include "Misc/FileHelper.h"


// Custom destructor for forward-declared ThespeonInference
void FThespeonInferenceDeleter::operator()(
    Thespeon::Inference::ThespeonInference* Ptr
) const
{
    delete Ptr;
}

UThespeonComponent::UThespeonComponent()
    : Super()
{
    PrimaryComponentTick.bCanEverTick = false;
    StreamComp = CreateDefaultSubobject<UAudioStreamComponent>(TEXT("AudioStream"));
    if (StreamComp)
    {
        StreamComp->bAutoActivate = false;
        StreamComp->bAutoDestroy  = false;
    }
}


// Destructor must be defined in .cpp file where ThespeonInference is a complete type
// This allows TUniquePtr to properly delete the forward-declared type
UThespeonComponent::~UThespeonComponent() = default;

void UThespeonComponent::OnRegister()
{
    Super::OnRegister();

    // StreamComp must be attached to the actor's root component, not to this ActorComponent
    if (!StreamComp) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    USceneComponent* Root = Owner->GetRootComponent();
    if (!Root)
    {
    // Create a default root if none exists
    Root = NewObject<USceneComponent>(Owner, TEXT("DefaultSceneRoot"));
    Owner->SetRootComponent(Root);
    Root->RegisterComponent();
    }

    // Only set up attachment if not already attached and not registered yet
    if (!StreamComp->IsRegistered())
    {
    StreamComp->SetupAttachment(Root);
    StreamComp->RegisterComponent();
    }
}

void UThespeonComponent::CleanupThread()
{
    if (SessionThread)
    {
        if (Session)
        {
            Session->Stop();
        }
        
        SessionThread->WaitForCompletion();

        delete SessionThread;
        SessionThread = nullptr;
    }

    if (Session)
    {
        Session = nullptr;
    }
}

// Called when component is unregistered - might be called without actually GC:ing the actor component
void UThespeonComponent::OnUnregister()
{
	// Detach audio stream component before unregistering to prevent self-attachment errors
	if (StreamComp && StreamComp->IsRegistered())
	{
		StreamComp->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
	
	Super::OnUnregister();
    CleanupThread();
}

// Called when actor component is GC:ed
void UThespeonComponent::BeginDestroy()
{
	Super::BeginDestroy();
    CleanupThread();
}

void UThespeonComponent::Synthesize(FLingotionModelInput Input, FString SessionId, FInferenceConfig InferenceConfig)
{
    if(bIsSynthesizing)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Synthesize called while already synthesizing. Please wait a moment. Ignoring this call."));
        return;
    }
    if(Input.Segments.Num() == 0)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Synthesize called with empty input segments. Ignoring this call."));
        return;
    }

    if(Input.Segments.Num() > 1)
    {
        LINGO_LOG(EVerbosityLevel::Error, TEXT("Synthesize called with multiple input segments (%d). This is currently unsupported."), Input.Segments.Num());
        return;
    }

    // Log the input data for debugging
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("Synthesize called with:"));
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("  Text: %s"), *Input.Segments[0].Text);
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("  ModuleSize: %s"), *UEnum::GetValueAsString(Input.ModuleType));
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("  CharacterName: %s"), *Input.CharacterName);
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("  Emotion: %d"), static_cast<int32>(Input.DefaultEmotion));
    if (!Input.DefaultLanguage.IsUndefined())
    {
        // somehow required for struct logging
        const FString LangName = Input.DefaultLanguage.ToString();
        LINGO_LOG(EVerbosityLevel::Debug, TEXT("  Language: %s"), *LangName);
    }

    // START THE AUDIO COMPONENT FIRST - before any audio data arrives
    if (StreamComp && !StreamComp->IsActive())
    {
        StreamComp->InputNumChannels = 1;
        StreamComp->OutputGain       = 1.0f;

        // Make sure it's initialized and registered
        if (!StreamComp->IsRegistered())
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("StreamComp not registered!"));
            return;
        }

        StreamComp->Activate(true);  // Activate the component
        StreamComp->Start();          // Start audio generation

        LINGO_LOG(EVerbosityLevel::Debug, TEXT("AudioStreamComponent started and activated"));
    }

    TWeakObjectPtr<UThespeonComponent> WeakThis(this);
    
    // Run inference on a background thread to avoid blocking the game thread

    // extra safety check to make sure that no previous session is left running
    CleanupThread();
    Session = TUniquePtr<Thespeon::Inference::ThespeonInference, FThespeonInferenceDeleter>
    (
        new Thespeon::Inference::ThespeonInference(Input, InferenceConfig, SessionId)
    );
    Session->OnDataSynthesized.BindUObject(WeakThis.Get(), &UThespeonComponent::PacketHandler);
   
    // creates and calls the FRunnable functions in the following order:
    // Init()->Run()->Stop()->Exit().
    SessionThread = FRunnableThread::Create(
        Session.Get(),
        TEXT("ThespeonBenchmarkThread"),
        0,
        TPri_AboveNormal
    );
    
    bIsSynthesizing = true;
}

bool UThespeonComponent::TryPreloadCharacter(FString CharacterName, EThespeonModuleType ModuleType, FInferenceConfig InferenceConfig)
{
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("ThespeonComponent TryPreloadCharacter called with CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));
    return Thespeon::Inference::ThespeonInference::TryPreloadCharacter(CharacterName, ModuleType, InferenceConfig);
}

bool UThespeonComponent::TryUnloadCharacter(FString CharacterName, EThespeonModuleType ModuleType)
{
    LINGO_LOG(EVerbosityLevel::Debug, TEXT("ThespeonComponent TryUnloadCharacter called with CharacterName: %s, ModuleType: %s"), *CharacterName, *UEnum::GetValueAsString(ModuleType));
    return Thespeon::Inference::ThespeonInference::TryUnloadCharacter(CharacterName, ModuleType);
}

void UThespeonComponent::PacketHandler(const Thespeon::Core::FAnyPacket& Packet, float BufferSeconds)
{
    if (const auto* F = Packet.TryGet<Thespeon::Core::FThespeonDataPacket<float>>())
    {
        if(!F->Metadata.bPacketStatus)
        {
            LINGO_LOG(EVerbosityLevel::Error, TEXT("Received packet with error status, dropping"));
            bIsSynthesizing = false;
            return;
        }
        PacketQueue.Enqueue(*F);
        CurrentDataLength += F->Data.Num();
        if(F->bIsFinalPacket || CurrentDataLength >= BufferSeconds * 44100)
        {
            LINGO_LOG(EVerbosityLevel::Debug, TEXT("Releasing queued audio packets, total samples: %d"), CurrentDataLength);
            TArray<float> AllData;
            AllData.SetNumUninitialized(CurrentDataLength);
            int32 Index = 0;
            Thespeon::Core::FThespeonDataPacket<float> P;
            while(PacketQueue.Dequeue(P))
            {
                FMemory::Memcpy(AllData.GetData() + Index, P.Data.GetData(), P.Data.Num() * sizeof(float));
                Index += P.Data.Num();
            }
            CurrentDataLength = 0;
            // call bound callback instead of streaming if possible
            if (OnAudioReceived.IsBound())
            {
                OnAudioReceived.Broadcast(AllData);
            }
            else if(F->Metadata.SessionID != TEXT("LINGOTION_WARMUP"))
            {
                StreamComp->SubmitAudio(AllData.GetData(), AllData.Num());
            }
        }
        if(F->bIsFinalPacket)
        {
            bIsSynthesizing = false;
        }
        return;

    }
   LINGO_LOG(EVerbosityLevel::Warning, TEXT("Unknown packet type received"));
}
