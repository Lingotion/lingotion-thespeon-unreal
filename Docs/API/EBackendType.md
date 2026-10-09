# Enum `EBackendType`

*Defined in: `LingotionThespeon/Public/Core/BackendType.h`*

Selects which NNE (Neural Network Engine) backend to use for inference.

## Values

### `CPU`
Run inference on the CPU.

### `GPU`
Run inference on the GPU. Windows only; other platforms fall back to CPU with a warning.

### `None`
Use the default backend specified in the Lingotion Thespeon runtime settings. Shown as "Default" in the editor.
