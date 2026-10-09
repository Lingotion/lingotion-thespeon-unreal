# Enum `EThespeonModuleType`

*Defined in: `LingotionThespeon/Public/Core/ModelInput.h`*

Quality tier of a Thespeon character module. Higher tiers produce better audio at the cost of increased computation.

## Values

### `None`
No module type selected. Synthesize uses the InferenceConfig's ModuleType (or the first imported one) instead; preload and unload calls fail.

### `XL`
Ultra-high quality. Best fidelity, highest resource usage.

### `L`
High quality. High fidelity, high resource usage.

### `M`
Medium quality. Balanced fidelity and performance.

### `S`
Low quality. Faster inference, reduced fidelity.

### `XS`
Ultra-low quality. Fastest inference, lowest fidelity.
