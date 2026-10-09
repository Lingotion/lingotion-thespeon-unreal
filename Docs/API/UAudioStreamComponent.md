# Class `UAudioStreamComponent`

*Defined in: `LingotionThespeon/Public/Utils/AudioStreamComponent.h`*

Streams synthesized audio to the Unreal audio engine in real time.
Receives 32-bit float audio on the game thread via SubmitAudioToStream() (or SubmitAudio() from C++),
e.g. from UThespeonComponent::OnAudioReceived, and feeds it to a custom ISoundGenerator for playback.
A lock-protected shared audio buffer bridges the game thread and the audio render thread.
The stream always runs at 44100 Hz.

## Functions

### `SubmitAudioToStream`
Submits interleaved 32-bit float audio data (44100 Hz) for streaming playback. Call from the game thread.
C++ callers with a raw buffer can use SubmitAudio(const float* Data, int32 NumSamples) instead.

**Parameters:**
- `AudioData`: Array of interleaved float samples.

```cpp
void SubmitAudioToStream(const TArray<float>& AudioData);
```

### `IsBufferEmpty`
Returns true when the shared audio buffer has no unconsumed samples remaining.
Intended use: after UThespeonComponent::OnSynthesisComplete fires, poll this to
detect when the audio stream has finished playing everything the synth produced.
A tiny DSP output latency may still be in flight when this first reports true.
Thread-safe: callable from the game thread while the audio render thread reads.

```cpp
bool IsBufferEmpty() const;
```

### `GetPlaybackSampleIndex`
Returns the number of audio sample frames that have been consumed by the audio render thread
since the stream started (or since the last ResetBuffer() call). Not reset per synthesis session:
call ResetBuffer() at session start to line up with per-session sample indices.
Thread-safe: callable from the game thread while the audio render thread writes.

**Parameters:**
- `CompensationSamples`: Optional number of sample frames to subtract, to compensate for output/device latency between a sample being consumed here and it becoming audible.

```cpp
int64 GetPlaybackSampleIndex(int32 CompensationSamples = 0) const;
```

### `ResetBuffer`
Clears the shared audio buffer and resets the playback position. Does not broadcast OnPlaybackBufferDrained.

```cpp
void ResetBuffer();
```

## Properties

### `InputNumChannels`
Number of interleaved channels in the submitted audio data. Read when the component initializes, so set it before then. Thespeon audio is
mono.

```cpp
int32 InputNumChannels = 1;
```

### `OutputGain`
Gain multiplier applied to the output audio signal (1.0 = unity, >1.0 = louder, <1.0 = quieter).
Default 2.0 compensates for the typically low peak amplitude of synthesized speech.
Override per-project in DefaultEngine.ini under [/Script/LingotionThespeon.AudioStreamComponent].
A changed value takes effect on the next SubmitAudio call.

```cpp
float OutputGain = 2.0f;
```

### `OnPlaybackBufferDrained`
Broadcast on the game thread when the audio buffer drains after having held audio.

```cpp
FOnPlaybackBufferDrained OnPlaybackBufferDrained;
```
