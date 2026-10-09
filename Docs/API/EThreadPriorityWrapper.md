# Enum `EThreadPriorityWrapper`

*Defined in: `LingotionThespeon/Public/Core/RuntimeThespeonSettings.h`*

Controls the OS priority of the synthesis thread. A higher priority helps real-time generation at the cost of higher resource use.
Preload threads always run at normal priority.

## Values

### `Normal`
Default OS thread priority.

### `AboveNormal`
Slightly elevated priority for smoother real-time generation.

### `BelowNormal`
Reduced priority to conserve resources.

### `Highest`
Maximum non-critical priority.

### `Lowest`
Minimum thread priority.

### `SlightlyBelowNormal`
Marginally below normal priority.

### `TimeCritical`
Highest possible priority. Use with caution as it may starve other threads.
