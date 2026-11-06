# Audio update 1.4.2

The ATM soundtrack gains DC removal, more output gain and bounded peak compression. Original scores and SFX overlays remain; classic BEEP mode is unchanged. The output helper is included locally and the existing save implementation is retained.

Validation uses the actual C++ audio backend and native Rev0 builds. Digital
levels and source checks do not establish measured acoustic loudness or
physical timer margin. These releases retain each public repository's
existing build and save setup; workspace console builds also included a
separate SD-save overlay that is not part of this audio change.

## Release build validation

Built against the clean upstream SDK at
`e1f8d05940ddb7bbef89c13326da75c003f02904` with GCC 16.2.

```text
MysticBalloon: build
flash (sections): 49713 B
image: 50200 B of 50944 (save pages free: 2; two need <= 50432)
static RAM: 18228 B of 18416 (stack 2048 B separate)
```

6 rendered audio files match the tested workspace PCM byte for byte.

All 15,303 original/native tracker states match, including SFX overlays.
