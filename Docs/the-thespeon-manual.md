# The Thespeon Manual

## Table of Contents
- [The Thespeon Manual](#the-thespeon-manual)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [The Thespeon Component](#the-thespeon-component)
    - [Adding the Component](#adding-the-component)
    - [Synthesizing a Performance](#synthesizing-a-performance)
    - [Preloading a Character](#preloading-a-character)
    - [Preloading a Character Group](#preloading-a-character-group)
    - [Cancelling a Synthesis](#cancelling-a-synthesis)
    - [Checking Synthesis State](#checking-synthesis-state)
    - [Resource Cleanup](#resource-cleanup)
  - [Character Control](#character-control)
    - [Input Building Blocks](#input-building-blocks)
    - [Mandatory Instructions](#mandatory-instructions)
    - [Optional Instructions](#optional-instructions)
    - [Fallback Handling](#fallback-handling)
    - [Language Matching](#language-matching)
    - [Custom Pronunciation (IPA)](#custom-pronunciation-ipa)
    - [Validating Your Input](#validating-your-input)
  - [Delegates and Events](#delegates-and-events)
    - [OnAudioReceived](#onaudioreceived)
    - [OnAudioSampleRequestReceived](#onaudiosamplerequestreceived)
    - [OnSynthesisComplete](#onsynthesiscomplete)
    - [OnPreloadComplete](#onpreloadcomplete)
    - [OnPreloadGroupComplete](#onpreloadgroupcomplete)
    - [OnSynthesisFailed](#onsynthesisfailed)
  - [Control Characters](#control-characters)
    - [Pause](#pause)
    - [Audio Sample Request](#audio-sample-request)
  - [Runtime Settings](#runtime-settings)
  - [Performance and Optimization](#performance-and-optimization)
    - [Module Type](#module-type)
    - [Thread Priority](#thread-priority)
    - [CPU Thread Count (ONNX Runtime)](#cpu-thread-count-onnx-runtime)
    - [Backend Selection: CPU vs GPU](#backend-selection-cpu-vs-gpu)
    - [Buffer Seconds](#buffer-seconds)
  - [Logging and Diagnostics](#logging-and-diagnostics)

---

## Overview

This document is the comprehensive knowledge repository for the Lingotion Thespeon plugin. It covers all major concepts, features, and behaviors of the plugin. For implementation examples, refer to the sample scenes included in the plugin. For complete API signatures and type definitions, refer to the [API documentation](./API/).

> [!IMPORTANT]
> Thespeon may be used both in Blueprint and C++. Its common starting point is the [`UThespeonComponent`](./API/UThespeonComponent.md), which you may attach to any Actor or use directly in a Level Blueprint.

Before reading this manual, we recommend completing the [Getting Started](./get-started-unreal.md) guide.

---

## The Thespeon Component

The [`UThespeonComponent`](./API/UThespeonComponent.md) is the central interface for all Thespeon functionality. All synthesis, preloading, cancellation, and event handling is done through this component.

### Adding the Component

`UThespeonComponent` is a `UActorComponent` that can be attached to any Actor in your level, or used directly in a Level Blueprint. It is a `BlueprintSpawnableComponent`, so it can be added through the editor's Add Component workflow or programmatically. Refer to the sample scenes included in the plugin for concrete setup examples in both Blueprint and C++.

### Synthesizing a Performance

The primary method of the Thespeon Component is `Synthesize`. You provide an instance of [`FLingotionModelInput`](./API/FLingotionModelInput.md) which details everything about the performance: what dialogue to speak, which character to use, and in what manner to perform it. You may also provide an optional session ID string to keep track of your synthesis session, and an optional [`FInferenceConfig`](./API/FInferenceConfig.md) instance to configure session-specific details such as whether to run on CPU or GPU.

The call initiates a background thread to leave your game thread unblocked.

> [!NOTE]
> By default, `Synthesize` streams the generated audio directly to a built-in audio streaming component for immediate playback. If you bind the `OnAudioReceived` delegate, this default behavior is replaced and you receive the raw audio data instead, giving you full control over how it is used. See the [Delegates and Events](#delegates-and-events) section for details.

If the requested character has not been preloaded, Thespeon will load it on demand before starting synthesis. This adds latency to the first call -- see [Preloading a Character](#preloading-a-character) for how to avoid this.

> [!NOTE]
> Parallel synthesis across multiple `UThespeonComponent` instances is supported via workload pooling. Within a single component, subsequent synthesis requests are queued, with syntheses being prioritized over pending preloads.

### Preloading a Character

While `Synthesize` will automatically load a character if needed, the loading process takes time and significantly affects real-time performance. To avoid this, you can preload characters ahead of time using `PreloadCharacter`. Parallel preloading is supported. Provide the character name, [`EThespeonModuleType`](./API/EThespeonModuleType.md), and optionally an `FInferenceConfig` to specify whether to load to the CPU or GPU backend.

`PreloadCharacter` is non-blocking -- the request is added to a queue and processed asynchronously. The `OnPreloadComplete` delegate fires when the operation finishes. See the GUISample Level Blueprint for an example of how preloading is done in practice.

> [!CAUTION]
> `PreloadCharacter` requires the specified character name and module type to exactly match an imported character module. Unlike `Synthesize`, it will not fall back to another character or module type if the specified combination is not found. Verify your imported modules in the Lingotion Thespeon Info window (`Window > Lingotion Thespeon Info`).

> [!NOTE]
> The backend chosen during preloading determines the backend on which `Synthesize` runs, regardless of any `FInferenceConfig` provided later to `Synthesize`.

The first synthesis for each character also has higher latency due to buffer initializations. Preloading with a mock synthesis beforehand can alleviate this -- the GUISample scene demonstrates this pattern.

### Preloading a Character Group

When you need to preload multiple characters at once, use `PreloadCharacterGroup` instead of calling `PreloadCharacter` in a loop. This method accepts an array of [`FPreloadEntry`](./API/FPreloadEntry.md) structs and a `PreloadGroupId` string. Each `FPreloadEntry` specifies a `CharacterName`, `ModuleType`, and optional `FInferenceConfig`.

All preloads in the group are registered atomically before any thread starts, ensuring the [`OnPreloadGroupComplete`](#onpreloadgroupcomplete) delegate cannot fire prematurely regardless of how fast individual preloads finish. The delegate fires once when all entries in the group have completed, providing the `PreloadGroupId` and a `bAllSucceeded` boolean indicating whether every entry succeeded.

Individual `OnPreloadComplete` delegates still fire for each entry in the group, so you can track per-character progress if needed.

> [!TIP]
> `PreloadCharacterGroup` is the preferred approach over calling `PreloadCharacter` in a loop with a shared group ID, because the atomic registration guarantees correct group completion tracking.

### Cancelling a Synthesis

If you need to stop an ongoing synthesis, call `CancelSynthesis` on the `UThespeonComponent`. This terminates the current synthesis session without broadcasting `OnSynthesisFailed`. The `OnSynthesisFailed` delegate is only broadcast when a synthesis actually fails, not when it is cancelled by the user.

### Checking Synthesis State

Use `IsSynthesizing` to check whether a synthesis session is currently in progress. It returns `true` if a session is actively running and `false` otherwise. This is useful for UI state management, preventing double-synthesis calls, or gating logic that should only run when synthesis is idle.

### Resource Cleanup

After synthesis completes, the loaded character module remains in memory until you explicitly call `TryUnloadCharacter` for that particular character module. This method removes references to the relevant resources so they can be reclaimed by the garbage collector. If you are done with a character and want to free memory, call `TryUnloadCharacter` with the character name, module type, and optionally the backend type.

> [!CAUTION]
> `TryUnloadCharacter` requires the specified character name and module type to exactly match a currently loaded character module. Unlike `Synthesize`, it will not fall back to another character or module type. If no loaded module matches the specified combination, the call fails and returns `false`.

---

## Character Control

This section details how to control the way a character speaks by constructing the [`FLingotionModelInput`](./API/FLingotionModelInput.md) struct. Think of it as the dialogue script provided to an actor prior to recording, detailing which character should say what and how.

### Input Building Blocks

To produce a performance, you construct an `FLingotionModelInput` instance that specifies your instructions. One instance corresponds to one line of dialogue. Since a full line may contain several sub-line specific instructions, `FLingotionModelInput` is composed of separate segments through a list of [`FLingotionInputSegment`](./API/FLingotionInputSegment.md) instances. To each segment you may specify performance details such as emotion and language.

For complete field definitions, see the API documentation for [`FLingotionModelInput`](./API/FLingotionModelInput.md) and [`FLingotionInputSegment`](./API/FLingotionInputSegment.md).

### Mandatory Instructions

While Thespeon is capable of filling in the blanks in the absence of instructions, there are a few parameters that must always be present for a synthesis to not be rejected:

1. `FLingotionModelInput.Segments` must contain at least one `FLingotionInputSegment`.
2. Each `FLingotionInputSegment` must have non-empty text.

### Optional Instructions

Aside from the mandatory instructions above, all other parameters are optional. If left unspecified, Thespeon falls back to default values (see [Fallback Handling](#fallback-handling)).

The most impactful instruction is the choice of character, which is selected by providing a `CharacterName` and `ModuleType` on the `FLingotionModelInput`. Together, they identify which of your imported character modules to use. If this pair does not match any available module, `Synthesize` selects the first available module it finds. Note that `PreloadCharacter` and `TryUnloadCharacter` do not perform this fallback. They require an exact match and will fail if the specified combination is not found among your imported modules.

On each segment you may also specify:

- `Emotion` -- the emotional tone for that segment, chosen from the [`EEmotion`](./API/EEmotion.md) enum.
- `Language` -- the language and dialect for that segment, specified as an [`FLingotionLanguage`](./API/FLingotionLanguage.md) struct.
- `bIsCustomPronounced` -- whether the segment text should be interpreted as IPA (International Phonetic Alphabet) rather than natural language. See [Custom Pronunciation (IPA)](#custom-pronunciation-ipa) for details.

> [!WARNING]
> For a character to speak a given language, the character must support it and you must also have imported the relevant language module into your Unreal project.

### Fallback Handling

When `Emotion` and `Language` are not specified on a segment, they are replaced by fallback values from increasingly broader scopes. The fallback chain is:

1. **Segment-level values** -- `FLingotionInputSegment.Emotion` and `FLingotionInputSegment.Language`
2. **Input-level defaults** -- `FLingotionModelInput.DefaultEmotion` and `FLingotionModelInput.DefaultLanguage`
3. **Config-level fallbacks** -- `FInferenceConfig.FallbackEmotion` and `FInferenceConfig.FallbackLanguage`
4. **Global defaults** -- The values configured in the [Runtime Settings](#runtime-settings) panel

### Language Matching

At synthesis time, each finally determined language instruction is matched to its closest available option if it is not an exact match. For example, if your character speaks English specifically with a Swedish accent and you provide the instruction "eng" for English, the character will assume you mean English with a Swedish accent and select the closest match.

The [`FLingotionLanguage`](./API/FLingotionLanguage.md) struct supports multiple code systems for precise language identification: ISO 639-2 (primary 3-letter code), ISO 639-3 (specific language code), Glottocode (family/dialect identifier), ISO 3166-1 and ISO 3166-2 (country and subdivision codes), and a `CustomDialect` field for free-form dialect specification.

### Custom Pronunciation (IPA)

If a segment is not marked as `bIsCustomPronounced`, Thespeon will internally convert its natural-language text into IPA (International Phonetic Alphabet) script before starting synthesis. Common words (~125 000 English words) are efficiently converted while uncommon words need some extra processing. Thespeon remembers every conversion it makes for each language family (based on its ISO639-2 code), as long as any character using that language family remains loaded in memory. This makes future conversions faster and more efficient.

When `bIsCustomPronounced` is set to `true` on a segment, the text field is directly interpreted as IPA, meaning a bypass of the conversion process. This allows precise control over pronunciation for names, technical terms, or non-standard words that the default text-to-phoneme conversion may not handle correctly. Furthermore the bypass means less processing and can as such be an effective way of reducing latency, especially for uncommon words.

### Validating Your Input

If you are unsure whether your instructions are clear enough for Thespeon to understand, you can check their validity using [`FLingotionModelInput::ValidateAndPopulate`](./API/FLingotionModelInput.md). This method inspects your `FLingotionModelInput` instance, auto-completes missing values, and replaces any invalid values with valid fallbacks. If any replacements are carried out, warnings are issued to the Thespeon logging system. To see these warnings, set the logging verbosity to at least Warning level in the [Runtime Settings](#runtime-settings).

> [!NOTE]
> `ValidateAndPopulate` only works properly if the intended character and module type are already loaded. You may use [`FLingotionModelInput::ValidateCharacterModule`](./API/FLingotionModelInput.md) to verify that your intended character is available in your project and update to an available one if not.

---

## Delegates and Events

The `UThespeonComponent` exposes a number of multicast delegates that you may bind your own functions to in order to respond to different signals from Thespeon. For complete delegate signatures, see the [UThespeonComponent API reference](./API/UThespeonComponent.md#delegates).

### OnAudioReceived

Broadcast whenever synthesized audio data is available during a synthesis session. The bound function receives the session ID and the raw audio data as an array of floats (44100 Hz mono).

Use this delegate to process, record, route, or visualize the audio as it is generated over time. We provide a basic audio streaming utility class `UAudioStreamComponent` that can be used for progressive playback as the audio data is synthesized. We also provide `FWavSaver`, a simple utility for saving generated audio data as a *.wav* file.

### OnAudioSampleRequestReceived

Broadcast when the audio sample indices corresponding to all [Audio Sample Request](#audio-sample-request) control characters in the input text have been computed. The delegate provides the session ID and an ordered array of sample indices (left-to-right marker order). This is guaranteed to broadcast before the first OnAudioReceived from the same synthesis session letting you properly schedule your events before audio starts streaming.

Combined with `OnAudioReceived`, this delegate enables precise synchronization of game events to specific moments in the spoken audio. See the [Audio Sample Request](#audio-sample-request) section for the full workflow.

### OnSynthesisComplete

Broadcast when the final audio packet has been delivered, indicating the synthesis session finished successfully. The delegate provides the session ID. This is useful for cleanup, advancing dialogue queues, or re-enabling UI elements.

### OnPreloadComplete

Broadcast when a `PreloadCharacter` operation completes. Useful for signaling a character is fully prepared for on-demand performance. The delegate provides:

- `PreloadSuccess` -- whether the preload succeeded or failed
- `CharacterName` -- the name of the character that was preloaded
- `ModuleType` -- the module type that was preloaded
- `BackendType` -- the backend to which the character was loaded

### OnPreloadGroupComplete

Broadcast when all preloads in a `PreloadCharacterGroup` call have completed. The delegate provides:

- `PreloadGroupId` -- the group ID string passed to `PreloadCharacterGroup`
- `bAllSucceeded` -- `true` if every individual preload in the group succeeded, `false` if any failed

This is useful for gating gameplay logic or UI state on the completion of an entire batch of preloads rather than tracking each one individually.

### OnSynthesisFailed

Broadcast when a synthesis has failed due to an error. The delegate provides the session ID of the failed synthesis. Useful for backup handling in case of failure.

---

## Control Characters

Thespeon provides special control characters that can be embedded in your input text to further control the synthesis output. These characters are accessible in Blueprint via the [`ULingotionBlueprintLibrary`](./API/ULingotionBlueprintLibrary.md) and in C++ via the `Thespeon::ControlCharacters` namespace. For complete API details, see the [ControlCharacters API reference](./API/ControlCharacters.md).

### Pause

The Pause control character tells Thespeon to insert a short pause of silence in the generated audio at the position where it appears in the text.

In Blueprint, use `ULingotionBlueprintLibrary::Pause()` (display name `GetPauseControlCharacter`) to obtain this character. In C++, use `Thespeon::ControlCharacters::Pause`.

The Pause character is useful for controlling pacing in dialogue -- for example, creating dramatic pauses, emphasizing sentence boundaries, or adding natural breathing points. When using [custom pronunciation (IPA)](#custom-pronunciation-ipa), the Pause character can still be inserted within IPA text to create deliberate pauses between phonetic sequences.

### Audio Sample Request

The Audio Sample Request control character marks a position in the input text for which Thespeon will report the corresponding audio sample index. This enables synchronization of game events to specific moments in the spoken dialogue.

In Blueprint, use `ULingotionBlueprintLibrary::AudioSampleRequest()` (display name `GetAudioSampleRequestControlCharacter`) to obtain this character. In C++, use `Thespeon::ControlCharacters::AudioSampleRequest`.

The event synchronization workflow is as follows:

1. Place Audio Sample Request markers in your input text at positions where you want to trigger game events (for example, immediately before a specific word).
2. Bind the [`OnAudioSampleRequestReceived`](#onaudiosamplerequestreceived) delegate on the `UThespeonComponent`.
3. When synthesis begins, `OnAudioSampleRequestReceived` fires with an array of audio sample indices corresponding to each marker, ordered from left to right in the input text.
4. If [`OnAudioReceived`](#onaudioreceived) is also bound, you receive the raw audio data and can track the current playback position by counting samples. When the sample count reaches one of the trigger indices, fire the corresponding game event.

This combination enables use cases such as lip sync, animation triggers, subtitle timing, or any event that must be synchronized to a specific word in the spoken dialogue.

> [!IMPORTANT]
> Currently, markers placed inside numerical or ordinal parts of the string are ignored. As a workaround, spell out the number in words and insert the marker as needed.

---

## Runtime Settings

Thespeon provides a set of global default settings that can be configured through the Unreal Editor at `Edit > Project Settings > Lingotion Thespeon (Runtime)`. These settings serve as the base defaults for `FInferenceConfig` -- when you create a new `FInferenceConfig` instance, it automatically reads from these settings. You can always override the global defaults by providing a specific `FInferenceConfig` to individual `Synthesize` or `PreloadCharacter` calls for more granular control.

The following settings are available:

| Setting | Description |
|---------|-------------|
| **Buffer Seconds** | How many seconds of audio should be synthesized until the first packet is sent |
| **Default Backend** | Preferred backend for inference (CPU or GPU) |
| **Default Module Type** | Preferred module type for inference |
| **Default Emotion** | Preferred emotion for inference |
| **Default Language** | Preferred language for inference |
| **Default Inference Thread Priority** | Thread priority for inference processing threads. Higher priority enables real-time generation at the cost of higher resource use |
| **Verbosity Level** | Controls the verbosity of Thespeon logging output |


> [!TIP]
> If you find yourself repeatedly passing the same `FInferenceConfig` values, consider setting your preferred defaults in the Runtime Settings instead. Every new default `FInferenceConfig` instance will automatically inherit these values.

---

## Performance and Optimization

Thespeon provides several levers for tuning the balance between audio quality, real-time performance, and resource usage.

### Module Type

The [`EThespeonModuleType`](./API/EThespeonModuleType.md) setting determines the size tier of the character module used for synthesis. The available sizes, from smallest to largest, are: `XS`, `S`, `M`, `L`, and `XL`. The sizes eligible _in your project_ are directly determined by the _.lingotion_ file(s) you chose to download.

Larger sizes produce better audio fidelity but require more computation and memory. Smaller sizes are faster and lighter but have reduced audio fidelity. Choosing the right size is a key performance lever -- for background NPCs, a smaller size may suffice, while main characters may warrant larger sizes. This is also a key factor to consider when building for different devices - large sizes will not be suitable for real-time generation if you expect target devices with limited compute capacity.


### Thread Priority

The `FInferenceConfig.ThreadPriority` setting (of type [`EThreadPriorityWrapper`](./API/EThreadPriorityWrapper.md)) controls the OS-level priority of the inference worker thread. A higher priority enables more real-time audio generation but increases resource contention with the game thread and other engine subsystems. A lower priority conserves resources but may cause audio to generate too slowly for real-time playback.

The available priority values are: `Lowest`, `BelowNormal`, `SlightlyBelowNormal`, `Normal`, `AboveNormal`, `Highest`, and `TimeCritical`. The default value is configured in the [Runtime Settings](#runtime-settings).

> [!WARNING]
> Setting the priority to `TimeCritical` may starve other threads on the system. Use it only when absolutely necessary and test thoroughly on your target hardware.

### CPU Thread Count (ONNX Runtime)

For inference on the CPU backend, the number of threads used by the ONNX Runtime can be configured under `Edit > Project Settings > NNERuntimeORT`. Two settings are available: `Intra-op thread count` and `Inter-op thread count`.

These settings influence the number of threads the ONNX Runtime may spawn during inference:

- A value of **0** means unconstrained thread usage, which will lead to FPS drops in Unreal Engine due to resource contention.
- A value **too low** may cause choppy audio because inference cannot generate audio fast enough for real-time playback.
- A value **too high** will cause the ONNX Runtime to compete with Unreal Engine for CPU time, leading to frame drops.

We recommend starting with **8 threads** for both settings and adjusting according to the rule of thumb above. The plugin ships with a `Config/DefaultEngine.ini` that can be imported in the Project Settings window to set these values.

### Backend Selection: CPU vs GPU

The [`EBackendType`](./API/EBackendType.md) setting determines the hardware backend used for inference. The available options are:

- `CPU` -- the default and safest option, works on all hardware.
- `GPU` -- can be faster on systems with capable GPUs, but depends on NNE GPU backend availability.
- `None` -- uses the default from [Runtime Settings](#runtime-settings).

> [!NOTE]
> To run a character on a selected backend the character must be loaded on it. Synthesize will always load what it needs if it does not already exist, which takes extra time. For lower latency, consider preloading the character on the intended backend ahead of time.

### Buffer Seconds

The `FInferenceConfig.BufferSeconds` setting controls how many seconds of audio are synthesized before the first audio packet is sent. A higher value increases initial latency but reduces the risk of audio stuttering during playback. A lower value reduces latency but may cause gaps if inference cannot keep up with playback speed.

---

## Logging and Diagnostics

Thespeon has its own logging system controlled by the Verbosity Level setting in the [Runtime Settings](#runtime-settings). The available levels, from least to most verbose, are: `None`, `Error`, `Warning`, `Info`, and `Debug`. Each level includes all messages from the levels above it.

Setting the verbosity to at least `Warning` during development is recommended. At this level, you will see warnings issued by `ValidateAndPopulate` when it auto-corrects invalid input values, as well as warnings from the language matching system when an exact language match is not found. These messages appear in the Unreal Engine Output Log under the Lingotion Thespeon category.
