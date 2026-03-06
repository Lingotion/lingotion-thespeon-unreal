# Class `FWavSaver`

*Defined in: `LingotionThespeon/Public/Utils/WavSaver.h`*

Utility for saving raw float PCM audio data to a .wav file on disk.

## Functions

### `SaveWav`
Writes an array of float samples as a 32-bit float WAV file.

**Parameters:**
- `Filename`: Absolute path for the output .wav file.
- `Samples`: The audio sample data (32-bit float, mono, 44100 Hz).

**Returns:** true if the file was written successfully.

```cpp
static bool SaveWav(const FString& Filename, const TArray<float>& Samples);
```
