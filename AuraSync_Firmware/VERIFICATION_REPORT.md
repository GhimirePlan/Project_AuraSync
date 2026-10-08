# Firmware Verification Report

## Completed checks

| Check | Result |
|---|---|
| Embedded model bytes equal supplied `.tflite` | Pass |
| Embedded model length | 77,384 bytes |
| Model SHA-256 | `5f687e9590ba40d5f1a6c8711aafd89d6d543f240bad4502263a936dfecb76ee` |
| Exported Hann window copied without numeric drift | Pass |
| Exported 64×257 Mel filterbank copied without numeric drift | Pass |
| Exported label order used by dashboard | Pass |
| Old custom TinyCNN weights/inference removed | Pass |
| `LOUD` and `SPIKE` excluded from CNN labels | Pass |
| Spike uses one-shot pulse and clear-to-rearm logic | Pass |
| Old `.git`, Keras training models, plots and dataset manifest excluded | Pass |

## Model evidence supplied by training export

- Deployment status: `APPROVED_FOR_PROTOTYPE`
- INT8 test accuracy: 0.8982
- INT8 balanced accuracy: 0.8861
- INT8 macro-F1: 0.8895
- Quantization accuracy drop: -0.0005

## Hardware verification still required

Automated checks here establish source/artifact consistency. Before the college
demonstration, compile and upload using the exact Arduino/board configuration in
`UPLOAD_GUIDE.md`, then confirm:

1. Serial prints `INT8 DS-CNN ready` and arena usage.
2. All three model probabilities update from live microphone audio.
3. One impulse flashes `SPIKE` briefly and then returns to the stable state.
4. Holding a loud/impulsive source does not hold or repeatedly count `SPIKE`.
5. A second impulse after a quiet gap creates a new blink and increments once.

Field accuracy and dBA calibration cannot be certified without the physical
ESP32-S3, microphone, enclosure, and reference meter.
