# Enum `EThreadPriorityWrapper`

*Defined in: `LingotionThespeon/Public/Core/RuntimeThespeonSettings.h`*

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

Thread Priority enum which controls the priority of synthesis threads. A higher priority will enable real time generation at the cost of higher
resource use.
