# **Plugin Samples**
The Lingotion Thespeon plugin ships with a set of samples. Each one is a small level or actor that shows one part of the API. They build on each other, so if you are new to Thespeon it is worth going through them roughly in the order listed below.

## Table of Contents
- [**Plugin Samples**](#plugin-samples)
  - [Table of Contents](#table-of-contents)
  - [Finding the samples](#finding-the-samples)
  - [Overview](#overview)
  - [Minimal Character](#minimal-character)
  - [Minimal Actor](#minimal-actor)
  - [Feature Showcase](#feature-showcase)
  - [Angel and Devil](#angel-and-devil)
  - [Lingotion GUI](#lingotion-gui)

---
## Finding the samples
1. Make sure the plugin is installed, your license key is added, and you have imported at least one character and its language pack(s). See [Get Started - Unreal](./get-started-unreal.md) if you have not done this yet.
2. The sample levels ship as plugin content. In the **Content Browser**, open:

   ```
   Plugins > Lingotion Thespeon > Samples > <Sample Name>
   ```
   If you do not see a **Plugins** folder, open the Content Browser **Settings** menu and enable **Show Plugin Content**.
3. The sample actors are C++ classes. You find them under **Plugins > Lingotion Thespeon C++ Classes > LingotionThespeon > Public > Samples**, or by searching for the class name in the **Place Actors** panel.
4. Open a sample level, or drag a sample actor into any level, and press **Play**.

Every sample follows the same pattern: a `UThespeonComponent` and a `UAudioStreamComponent` sit on the same actor, `OnAudioReceived` is bound, and each audio packet is passed to `SubmitAudioToStream` as it arrives so playback starts before the whole line has been generated.

## Overview
| Sample | Kind | Shows | Read alongside |
|---|---|---|---|
| [Minimal Character](#minimal-character) | Level Blueprint | The least wiring needed to synthesize and play a line | [Your first synthesis in Blueprint](./get-started-unreal.md#your-first-synthesis-in-blueprint) |
| [Minimal Actor](#minimal-actor) | C++ actor | The same thing in C++, as a ready-made actor configured from the Details panel | [Your first synthesis in C++](./get-started-unreal.md#your-first-synthesis-in-c) |
| [Feature Showcase](#feature-showcase) | Level Blueprint | Preloading, input validation, control characters, audio sample requests, failure handling and unloading | [The Thespeon Manual](./the-thespeon-manual.md) |
| [Angel and Devil](#angel-and-devil) | C++ actor | Several characters preloading and speaking at the same time, each with its own emotion | [Preloading a Character](./the-thespeon-manual.md#preloading-a-character) |
| [Lingotion GUI](#lingotion-gui) | UMG + C++ | An interactive runtime panel for trying out every delivery control | [Blending emotions and shaping delivery](./the-thespeon-manual.md#blending-emotions-and-shaping-delivery) |

---
## Minimal Character
**Level:** `Samples/MinimalCharacterSample/MinimalCharacter`

The bare minimum, built entirely in the Level Blueprint. On **BeginPlay** it adds a Thespeon Component and an Audio Stream Component, starts the audio stream, binds `OnAudioReceived` to `SubmitAudioToStream`, and calls `Synthesize` with a single segment: *"Hi! This is my voice generated in real-time!"*, using the *Interest* emotion and an `L` module.

There is no preloading, so the first line pays the loading cost. Open the Level Blueprint to change the character, text or emotion on the **Make LingotionModelInput** node.

## Minimal Actor
**Level:** `Samples/MinimalActorExample/MinimalActor` &nbsp; **Class:** `ASimpleThespeonActor`

The C++ version of the Minimal Character sample. `ASimpleThespeonActor` creates both components in its constructor, binds `OnAudioReceived` in `BeginPlay`, and synthesizes one line using the settings in its **Thespeon Test Config** category:

| Setting | Purpose |
|---|---|
| *Test Character Name* | The character to speak. Must match an imported character. |
| *Test Module Type* | The module size to use. |
| *Test Language* | The language, set both as the input default and on the segment. |
| *Test Text To Synthesize* | The line to speak. |
| *Auto Synthesize On Begin Play* | Synthesize the line one second after **BeginPlay**. Off by default. |

The level contains one instance of the actor. You can also drop `ASimpleThespeonActor` into any level of your own. Read [SimpleThespeonActor.cpp](../Source/LingotionThespeon/Private/Samples/MinimalActorExample/SimpleThespeonActor.cpp) alongside [Your first synthesis in C++](./get-started-unreal.md#your-first-synthesis-in-c).

## Feature Showcase
**Level:** `Samples/FeatureShowcase/FeatureTesting`

A Level Blueprint that goes through the full lifecycle of a line, with each step labelled by a comment in the graph:
- **Choose a character** by editing the variables at the top of the graph (Deryn Oliver by default).
- **Preload** the chosen character on **BeginPlay** and wait for `OnPreloadComplete` before synthesizing.
- **Validate** the input with `ValidateCharacterModule`, which falls back to another module size if the requested one is not imported.
- **Build an input that tests several features**: multiple segments, the `Pause` and `AudioSampleRequest` control characters, and the *Is Custom Pronounced* flag for IPA.
- **Receive audio sample requests** through `OnAudioSampleRequestReceived` and print *"Received trigger number N"* as playback reaches each marker.
- **Handle failure** with `OnSynthesisFailed`.
- **Unload** the character with `TryUnloadCharacter` once `OnSynthesisComplete` fires.

The inference config asks for the GPU backend. GPU inference is only available on Windows, and other platforms fall back to CPU automatically (see [Backend Selection: CPU vs GPU](./the-thespeon-manual.md#backend-selection-cpu-vs-gpu)).

## Angel and Devil
**Class:** `AAngelDevilDemoActor`

There is no level for this sample. Drag `AAngelDevilDemoActor` into any level and press **Play**. The actor plays three roles that respond to the same moral dilemma (*"I found a wallet on the ground. What should I do?"*), each with its own emotion:

| Role | Emotion | Line |
|---|---|---|
| Person | Interest | Asks the question. |
| Angel | Serenity | Advises returning the wallet. |
| Devil | Anger | Advises keeping the money. |

Each role has its own Thespeon Component and Audio Stream Component, and all three preload at the same time on **BeginPlay**. Status, preload times and packet counts are shown as on-screen messages.

| Key | Action |
|---|---|
| **1** / **2** / **3** | Synthesize the Person, Angel or Devil line |
| **4** | Synthesize all three at once |
| **5** | Cancel all |

Each role's character, emotion and text can be changed under **Demo Config > Roles** in the Details panel, along with a shared *Module Type* (`M` by default) and *Language* (English by default). By default all three roles use **Aaron Archer**; give each role its own character to hear three different voices. Enable *Auto Synthesize On Preload Complete* to start the conversation as soon as loading finishes.

## Lingotion GUI
**Level:** `Samples/LingotionGUISample/L_LingotionGUISample` &nbsp; **Widget:** `W_ThespeonGUI` &nbsp; **Class:** `UAdvancedThespeonWidget`

A runtime control panel for experimenting with a character without writing code. This is the level the [Get Started guide](./get-started-unreal.md#run-the-guisample-level) points you to. Open it, press **Play**, and use the panel to:
- pick a **character**, **module size**, **backend** (Default, CPU or GPU) and **language**. The choices cascade, so only what the selected character supports is offered,
- type the **line** to speak (up to 250 characters),
- set the **start and end emotions** as blends in the emotion editor, and the **start and end speed and loudness**,
- press **Synthesize**. The status line below the button reports when the line is submitted, completed, cancelled or has failed.

While the line plays, a dialogue overlay shows the character's portrait and reveals the line word by word in time with the audio. It puts an `AudioSampleRequest` marker in front of every word and compares the sample indices it gets back from `OnAudioSampleRequestReceived` against the playback position. Close the overlay to cancel the line. The overlay also starts preloading every imported character and module on the CPU backend as soon as it is constructed, which is when the level starts and before the first line is requested. Later lines then start quickly, but with many modules imported this preload takes a while and uses memory for every module.

**Developer mode.** Press **Ctrl+D** to show or hide the segment controls, which split the line into several segments, each with its own text, language and boundary values, to build a curve with more than two keypoints. Developer mode is on by default. Turning it off merges the segments back into one.

The widgets have a few Class Defaults settings:
| Widget | Setting | Purpose |
|---|---|---|
| `W_ThespeonGUI` | *Character Portraits* | Portrait textures for specific characters, keyed by character name. |
| `W_ThespeonGUI` | *Default Character Portrait* | Used for any character not listed above. |
| `W_ThespeonGUI` | *Max Input Text Length* | Character limit per segment. 0 disables the limit. |
| `W_ThespeonGUI` | *Max Summary Chips* | How many emotions to show in the start/end summary before collapsing the rest into one "N more" chip. |
| `W_ThespeonGUI` | *Session Id Prefix* | Prefix for the session ID passed to `Synthesize`. A unique suffix is added per line. |
| `W_ThespeonDialogueOverlay` | *Auto Close Delay* | How long the overlay stays up after the line finishes. |
| `W_ThespeonDialogueOverlay` | *Playback Latency Compensation Samples* | Raise if the text runs ahead of the audio. |
| `W_ThespeonDialogueOverlay` | *Loading Dot Interval* | Speed of the loading animation shown before the first word. |

The layout and styling live in the Widget Blueprints under `Samples/LingotionGUISample/Widgets`, and the behaviour lives in the C++ classes under `Source/LingotionThespeon/Private/Samples/LingotionGUISample`, so you can restyle the panel without touching code.
