# Namespace `Thespeon::ControlCharacters`

*Defined in: `LingotionThespeon/Public/Core/ModelInput.h`*

## Properties

### `Pause`
This character tells Thespeon to insert a short silence in the generated dialogue.

```cpp
constexpr TCHAR Pause = TEXT('⏸');
```

### `AudioSampleRequest`
Thespeon can find the audio sample that best corresponds to a position in the input text. Place this character in the text to request
the audio sample index at that position. The OnAudioSampleRequestReceived delegate delivers all such sample indices in left-to-right order.
It is guaranteed to broadcast before the first OnAudioReceived for the same synthesis session.

```cpp
constexpr TCHAR AudioSampleRequest = TEXT('◎');
```
