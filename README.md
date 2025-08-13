# VS-AudioTools
Some basic audio functions for VapourSynth.

[Convert](#convert)  
[Crossfade](#crossfade)  
[Delay](#delay)  
[FadeIn](#fadein)  
[FadeOut](#fadeout)  
[FindPeak](#findpeak)  
[Mix](#mix)  
[Normalize](#normalize)  
[SineTone](#sinetone)

[Overflow handling](#overflow-handling)

[Build from source](#build-from-source)  
[License](#license)



## Convert
Convert the sample type.

```python
atools.Convert(clip: vs.AudioNode,
               sample_type: str,
               overflow: str = 'error',
               overflow_log: str = 'once'
               ) -> vs.AudioNode
```

*clip* - input audio clip

*sample_type* - sample type of the output clip
```text
    'i16' - integer 16-bit
    'i24' - integer 24-bit
    'i32' - integer 32-bit
    'f32' - float   32-bit
```

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## Crossfade

Crossfade two audio clips.
The output clip has the length: len(clip1) + len(clip2) - crossfade_samples

```python
atools.Crossfade(clip1: vs.AudioNode,
                 clip2: vs.AudioNode,
                 samples: int = 0,
                 seconds: float = 0.0,
                 type: str = 'cubic',
                 overflow: str = 'error',
                 overflow_log: str = 'once'
                 ) -> vs.AudioNode
```

*clip1* - first audio clip

*clip2* - second audio clip (same format as clip1)

*samples* - crossfade length in samples

*seconds* - crossfade length in seconds

*type* - fade transition type
```text
    'linear' - linear transition
    'cubic'  - cubic transition
    'sine'   - sine transition
```

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## Delay

Delay (or shift) an audio clip. The output length stays the same.  
This effect could be also achieved with scripting.

```python
atools.Delay(clip: vs.AudioNode,
             samples: int = 0,
             seconds: float = 0.0,
             channels: list[int] = None,
             overflow: str = 'error',
             overflow_log: str = 'once'
             ) -> vs.AudioNode
```

*clip* - input audio clip

*samples* - delay length in samples

*seconds* - delay length in seconds

*channels* - list of channels to delay; default: None (all channels)

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## FadeIn

Fade in an audio clip.

```python
atools.FadeIn(clip: vs.AudioNode,
              samples: int = 0,
              seconds: float = 0.0,
              start_sample: int = 0,
              start_second: float = 0.0,
              channels: list[int] = None,
              type: str = 'cubic',
              overflow: str = 'error',
              overflow_log: str = 'once'
              ) -> vs.AudioNode
```

*clip* - input audio clip

*samples* - fade in length in samples

*seconds* - fade in length in seconds

*start_sample* - sample to start fading in

*start_second* - second to start fading in

*channels* - list of channels to fade in; default: None (all channels)

*type* - fade transition type
```text
    'linear' - linear transition
    'cubic'  - cubic transition
    'sine'   - sine transition
```

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## FadeOut

Fade out an audio clip.

```python
atools.FadeOut(clip: vs.AudioNode,
               samples: int = 0,
               seconds: float = 0.0,
               end_sample: int = len(clip),
               end_second: float = to_seconds(len(clip)),
               channels: list[int] = None,
               type: str = 'cubic',
               overflow: str = 'error',
               overflow_log: str = 'once'
               ) -> vs.AudioNode
```

*clip* - input audio clip

*samples* - fade out length in samples

*seconds* - fade out length in seconds

*end_sample* - sample to end fading out (exclusive!)

*end_second* - second to end fading out (exclusive!)

*channels* - list of channels to fade out; default: None (all channels)

*type* - fade transition type
```text
    'linear' - linear transition
    'cubic'  - cubic transition
    'sine'   - sine transition
```

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## FindPeak

Return the peak value of all audio samples. This function is not an audio filter.

**Note**: Calling this function will read all audio frames in advance, which is a blocking process
          and can take a while to complete depending on the audio length.

```python
atools.FindPeak(clip: vs.AudioNode,
                normalize: bool = True,
                channels: list[int] = None
                ) -> float
```

*clip* - input audio clip

*normalize* - if True returns a normalized peak value between 0 and 1,  
              otherwise returns the exact peak sample value; default: True

*channels* - list of channels to read; default: None (all channels)


## Mix

Mix two audio clips together. Optionally fade in / fade out clip2 respectively clip1 depending on the offset of clip2.  
This is a convenience function and can also be achieved with existing functions and scripting.

**Note**: This function is prone to overflowing. Please see the section about how to [handle overflows](#overflow-handling).

```python
atools.Mix(clip1: vs.AudioNode,
           clip2: vs.AudioNode,
           clip2_offset_samples: int = 0,
           clip2_offset_seconds: float = 0.0,
           clip1_gain: float = 1.0,
           clip2_gain: float = 1.0,
           relative_gain: bool = False,
           fadein_samples: int = 0,
           fadein_seconds: float = 0.0,
           fadeout_samples: int = 0,
           fadeout_seconds: float = 0.0,
           fade_type: str = 'cubic',
           extend_start: bool = False,
           extend_end: bool = False,
           channels: list[int] = None,
           overflow: str = 'error',
           overflow_log: str = 'once'
           ) -> vs.AudioNode
```

*clip1* - base input audio clip

*clip2* - audio clip to mix into clip1 (same format as clip1)

*clip2_offset_samples* - sample position (relative to clip1) of where to mix clip2 into clip1;
                         can be negative

*clip2_offset_seconds* - time in seconds (relative to clip1) of where to mix clip2 into clip1;
                         can be negative

*clip1_gain* - apply gain to clip1

*clip2_gain* - apply gain to clip2

*relative_gain* -  if true clip1_gain and clip2_gain are relative values and the absolute gains will add up to 1;  
e.g. if clip1_gain is 1 and clip2_gain is 4 then the absolute gain for clip1 will be 0.2 and for clip2 will be 0.8;  
this can be used to prevent overflowing, but should not be used if you want to call Mix more than once, because it lowers the overall volume every time you call Mix;  
default: False

*fadein_samples* - fade in length in samples

*fadein_seconds* - fade in length in seconds

*fadeout_samples* - fade out length in samples

*fadeout_seconds* - fade out length in seconds

*fade_type* - fade transition type
```text
    'linear' - linear transition
    'cubic'  - cubic transition
    'sine'   - sine transition
```

*extend_start* - if the start of clip2 is outside of clip1 (negative clip2_offset) you can choose
                 to extend the start of clip1 to cover the beginning of clip2, which increases
                 the output length; default: False

*extend_end* - if the end of clip2 is outside of clip1 you can choose to extend the end of clip1
               to cover the end of clip2, which increases the output length; default: False

*channels* - list of channels of clip2 to mix in; default: None (all channels)

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## Normalize

Simple peak normalization.  
Applies a gain to the input clip to match the desired normalized peak value.

**Note**: Calling this function will read all audio frames in advance, which is a blocking process
          and can take a while to complete depending on the audio length.

```python
atools.Normalize(clip: vs.AudioNode,
                 peak: float = 1.0,
                 lower_only: bool = False,
                 channels: list[int] = None,
                 overflow: str = 'error',
                 overflow_log: str = 'once'
                 ) -> vs.AudioNode
```

*clip* - input audio clip

*peak* - normalized peak value to scale the audio to; default: 1.0

*lower_only* - only reduce the volume to match the desired peak value; default: False

*channels* - list of channels to normalize; default: None (all channels)

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## SineTone

Create a constant beeping tone clip.

```python
atools.SineTone(clip: vs.AudioNode = None,
                samples: int = 10 * sample_rate
                seconds: float = 10.0,
                sample_rate: int = 44100,
                sample_type: str = 'i16',
                freq: float = 500.0,
                amp: float = 1.0,
                channels: list[int] = [vs.FRONT_LEFT, vs.FRONT_RIGHT],
                overflow: str = 'error',
                overflow_log: str = 'once'
                ) -> vs.AudioNode
```

*clip* - use audio format from this clip; values can be overwritten with the parameters below

*samples* - audio length in samples

*seconds* - audio length in seconds

*sample_rate* - sample rate of the output clip

*sample_type* - sample type of the output clip
```text
    'i16' - integer 16-bit
    'i24' - integer 24-bit
    'i32' - integer 32-bit
    'f32' - float   32-bit
```

*freq* - frequency of the sine tone

*amp* - amplitude of the sine tone

*channels* - channels for the channel layout; default: [vs.FRONT_LEFT, vs.FRONT_RIGHT]

*overflow* - sample overflow handling; default: 'error' - see [explanation below](#overflow-handling)

*overflow_log* - sample overflow logging; default: 'once' - see [explanation below](#overflow-handling)


## Overflow handling

All functions have an 'overflow' parameter that determines how to handle overflows,  
and an 'overflow_log' parameter that determines how to log overflows.

*overflow* - sample overflow handling; default: 'error'
```text
    'error'      - raise an error (default)
    'clip'       - clip overflowing samples (all types)
    'clip_int'   - clip overflowing samples for integer output sample types
                   keep overflowing samples for float output sample types
    'keep_float' - keep overflowing samples for float output sample types
                   raise an error if output sample type is not float
```

To properly handle overflows the clip should be converted to a float sample type first ('f32'), if not already.

⚠️ Overflowing samples of integer output sample types are always clipped (disruptive), or they raise an error

Use `overflow='keep_float'` for float output sample types to leave overflowing samples unchanged.  
Then call a scaling function like `atools.Normalize` or `std.AudioGain` that scales the peak sample value below or to equal 1.0 (see [Example](#example))

*overflow_log* - sample overflow logging; default: 'once'
```text
    'all'  - log all sample overflows (not recommended, this can be a lot)
    'once' - log only the first sample overflow (default)
    'none' - do not log any sample overflows
```

**Note**: a summary of all overflowing samples will be logged at the end of each function (if any)


### Example

```python
import vapoursynth as vs

# load audio (integer or float sample type)

# e.g. create a new integer sample type clip
audio = vs.core.atools.SineTone(sample_type='i24')

# convert sample type to 32-bit float
audio = vs.core.atools.Convert(audio, sample_type='f32')

# process audio

# make the audio overflow
audio = vs.core.std.AudioGain(audio, 1.2)

# apply FadeIn and FadeOut to the overflowing clip
# keep overflowing samples with 'keep_float'
audio = vs.core.atools.FadeIn(audio, seconds=2.0, overflow='keep_float')
audio = vs.core.atools.FadeOut(audio, seconds=2.0, overflow='keep_float')

# fix overflows

# scale peak to 1.0 with Normalize
audio = vs.core.atools.Normalize(audio)

# convert sample type back to integer if needed
audio = vs.core.atools.Convert(audio, sample_type='i24')

# Note: Normalize and Convert would both raise an error if any overflow would occur
#       on their output samples, since the default value for overflow is 'error'
```

## Dependencies
None


## Build from source
Use cmake to configure your preferred build system and run it.  
e.g. cmake with Ninja:
```sh
cmake -G Ninja -B ./build-ninja -DCMAKE_BUILD_TYPE=Release

ninja -C ./build-ninja
```


## License
This project is licensed under the MIT License.
