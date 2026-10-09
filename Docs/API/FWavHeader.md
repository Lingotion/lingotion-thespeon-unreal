# Struct `FWavHeader`

*Defined in: `LingotionThespeon/Public/Utils/WavSaver.h`*

Standard WAV file header (44 bytes, packed).
Defaults to 32-bit IEEE 754 float, mono, 44100 Hz (AudioFormat = 3).
ChunkSize, ByteRate, BlockAlign, and Subchunk2Size are computed at write time.

## Properties

### `ChunkID`
RIFF chunk identifier, always "RIFF".

```cpp
char ChunkID[4] = {'R', 'I', 'F', 'F'};
```

### `ChunkSize`
Size of the rest of the file after this field: 36 + Subchunk2Size. Computed at write time.

```cpp
uint32 ChunkSize = 0;
```

### `Format`
RIFF form type, always "WAVE".

```cpp
char Format[4] = {'W', 'A', 'V', 'E'};
```

### `Subchunk1ID`
Format chunk identifier, always "fmt " (with a trailing space).

```cpp
char Subchunk1ID[4] = {'f', 'm', 't', ' '};
```

### `Subchunk1Size`
Size of the format chunk that follows: 16 bytes.

```cpp
uint32 Subchunk1Size = 16;
```

### `AudioFormat`
Audio format: 3 = IEEE 754 float.

```cpp
uint16 AudioFormat = 3;
```

### `NumChannels`
Number of interleaved channels: 1 (mono).

```cpp
uint16 NumChannels = 1;
```

### `SampleRate`
Sample frames per second: 44100.

```cpp
uint32 SampleRate = 44100;
```

### `ByteRate`
Bytes per second: SampleRate * BlockAlign. Computed at write time.

```cpp
uint32 ByteRate = 0;
```

### `BlockAlign`
Bytes per sample frame across all channels: NumChannels * 4. Computed at write time.

```cpp
uint16 BlockAlign = 0;
```

### `BitsPerSample`
Bits per sample: 32.

```cpp
uint16 BitsPerSample = 32;
```

### `Subchunk2ID`
Data chunk identifier, always "data".

```cpp
char Subchunk2ID[4] = {'d', 'a', 't', 'a'};
```

### `Subchunk2Size`
Size of the sample data in bytes. Computed at write time.

```cpp
uint32 Subchunk2Size = 0;
```
