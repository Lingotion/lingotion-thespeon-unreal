# [1.2.0] - 2026-09-15
This version requires updated character and language modules, please re-download your characters to enable the new functionality.

## Added
* Added interpolation of emotions over time by setting the `StartEmotion` and `EndEmotion` parameters of a `FLingotionInputSegment`.
* Added blending of emotions, which combined with the interpolation makes it possible to blend emotions over time.
* Added speeds input parameters
* Added loudness input parameters
* New GUI Sample with refreshed interface that showcases emotion blending.
* Additional instructions on how to get started with plugin.
* Added fallback for languages in the case that a specific language module does not exist.

## Changed
* It is now possible to preload on multiple backends at the same time.

## Removed
* Removed the old GUISample level

## Fixed
* Fixed an issue on Windows where running synth on GPU sometimes would produce garbage in the end of the audio.
* Fixed a crash when exiting Play mode during preload
* Adjusted gain level for `AudioStreamComponent`

# [1.1.1] - 2026-04-29
## Added
* Concurrent character preloading via GUI sample.
* Additional instructions on how to get started with plugin.

# [1.1.0] - 2026-03-30
## Added
* Parallel synthesis across multiple `UThespeonComponent` instances via workload pooling.
* Concurrent character preloading — sppeding up the preload process significantly.
* `PreloadCharacterGroup` method for atomic batch preloading with group completion tracking.
* `OnPreloadGroupComplete` delegate — fires when all preloads in a group complete.
* `FLingotionModelInput::ValidateCharacterModule` and `ValidateAndPopulate` for input validation with fallbacks.
* `AngelDevilDemoActor` — demo actor showcasing multi-character concurrent synthesis.
* Async preload system with `FPreloadSession` for non-blocking character loading.

## Changed
* Thread-safe subsystem infrastructure — all manager subsystems now use locks and shared pointers for safe concurrent access.
* Delegates (`OnAudioReceived`, `OnSynthesisComplete`, etc.) moved from class scope to file scope.
* `UPROPERTY` raw `UObject*` pointers converted to `TObjectPtr` in actor headers.
* Demo GUI layout updated.

## Fixed
* Crash when exiting Play mode during preload.
* `InferenceWorkload::Infer` now returns failure status instead of silently continuing on model run failure.
* Faulty GPU-related text corrected.
* `IsInGameThread()` check moved earlier to prevent incorrect thread assertions.

## Removed
* Unused `UAudioStreamComponent* StreamComp` from `ThespeonComponent`.

# [1.0.0] - 2026-03-06
## Added
* First major release of Lingotion Thespeon Unreal Plugin.
* New model loading system, making it possible to easily swap out new models in the future.

# [0.1.0] - 2025-12-12
## Added
* First beta release of the Lingotion Thespeon Unreal Plugin.
