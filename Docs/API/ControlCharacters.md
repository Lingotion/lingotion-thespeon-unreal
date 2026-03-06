# Namespace `Thespeon::ControlCharacters`

*Defined in: `LingotionThespeon/Public/Core/ModelInput.h`*

## Properties

### `Pause`
This character tells Thespeon to insert a short pause of silence in the generated dialogue.

```cpp
TCHAR Pause = TEXT('⏸');
```

### `AudioSampleRequest`
Thespeon is able to find the audio sample which best corresponds to a position in the input text. This character marks one such position to request
its corresponding audio sample. The FOnAudioSampleRequestReceived delegate will deliver all such sample indices in left-to-right order.
It is guaranteed to broadcast before the first FOnAudioReceived for the same synthesis session.

```cpp
TCHAR AudioSampleRequest = TEXT('◎');
```
