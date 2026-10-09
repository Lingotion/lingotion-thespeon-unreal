# Class `FWavSaver`

*Defined in: `LingotionThespeon/Public/Utils/WavSaver.h`*

Utility for saving raw 32-bit float audio data to a .wav file on disk.

## Functions

### `SaveWav`
Writes an array of float samples as a 32-bit float WAV file.

**Parameters:**
- `Filename`: Output path. An existing file is overwritten.
- `Samples`: The audio sample data (32-bit float, mono, 44100 Hz).

**Returns:** true if the file was written; false if Samples is empty (with an error logged) or the file write fails.

```cpp
static bool SaveWav(const FString& Filename, const TArray<float>& Samples);
```
