# AuraSync INT8 DS-CNN Model Card

## Intended use

The model estimates the dominant one-second acoustic context:

1. `stationary_background`
2. `speech_dominant`
3. `non_speech_activity`

It does not identify words, speakers, or people. It does not determine physical
loudness or spikes; those are independent calibrated DSP decisions.

## Deployment model

- Version: 2.0.0
- Format: fully quantized INT8 TFLite
- Size: 77,384 bytes
- Input audio: 16,000 mono PCM samples at 16 kHz
- Input features: 49 time frames × 64 log-Mel bands × 1 channel
- FFT/window/hop: 512 / 512 / 320 samples
- Frequency range: 80–7,600 Hz
- Classes: 3
- Live confidence threshold exported by training: 0.34

Firmware uses the exported Hann window, Mel matrix, normalization statistics,
and tensor quantization values. The classifier is therefore no longer the old
16×17 handcrafted-feature showcase network.

## Held-out performance

| Metric | Float student | INT8 deployment |
|---|---:|---:|
| Accuracy | 0.8977 | 0.8982 |
| Balanced accuracy | — | 0.8861 |
| Macro-F1 | 0.8890 | 0.8895 |

Dataset counts: 8,750 train, 1,840 validation, and 1,876 test clips. The training
export marks the deployment `APPROVED_FOR_PROTOTYPE`.

## Limitations

- Held-out accuracy is not a guarantee on a new microphone or enclosure.
- The package requires live field tests and physical dBA calibration.
- `stationary_background` and `non_speech_activity` may overlap acoustically.
- This is a prototype research classifier, not a safety-certified alarm.

## Provenance

Dataset repository: `GhimirePlan/AS_Final_Dataset_`, commit
`f68c273f7ffdf79bb8750ff54e6c7280790b3d01`. Metrics and hashes are retained in
the `model/` directory.
