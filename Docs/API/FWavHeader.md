# Struct `FWavHeader`

*Defined in: `LingotionThespeon/Public/Utils/WavSaver.h`*

Standard WAV file header (44 bytes, packed).
Defaults to 32-bit float mono PCM at 44100 Hz (AudioFormat = 3 = IEEE float).
ChunkSize, ByteRate, BlockAlign, and Subchunk2Size are computed at write time.

## Properties

### `AudioFormat`
Audio format: 3 = IEEE 754 float.

```cpp
uint16 AudioFormat = 3;
```
