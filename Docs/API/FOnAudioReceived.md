# Delegate `FOnAudioReceived`

*Defined in: `LingotionThespeon/Public/Engine/ThespeonComponent.h`*

Signature of UThespeonComponent::OnAudioReceived, broadcast on the game thread whenever a chunk of
synthesized audio is ready. The component does not play audio itself; pass the samples to e.g.
UAudioStreamComponent::SubmitAudioToStream for playback.

**Parameters:**
- `SessionID`: The session ID passed to Synthesize.
- `SynthesisData`: Mono 32-bit float samples at 44100 Hz.

## Declaration

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAudioReceived, FString, SessionID, const TArray<float>&, SynthesisData);
```
