# AuraSync ESP32-S3 Firmware — Trained INT8 DS-CNN

 firmware for ESP32-S3 N8R8 + INMP441. It combines calibrated
DSP, the exported three-class INT8 TensorFlow Lite Micro model, a stable fusion
state machine, and an offline dashboard served by the board.

## What is real in this build

- Model: `aurasync_3class_ds_cnn_.tflite`,
- Classes: `stationary_background`, `speech_dominant`, `non_speech_activity`
- Input: one second of 16 kHz mono audio
- Firmware preprocessing: 512-point FFT, 320-sample hop, exported 64-band Mel
  filterbank, natural log, clip-mean normalization, exported per-Mel mean/std
- Tensor: INT8 `[1,49,64,1]`; output: INT8 probabilities for three classes
- Independent INT8 test accuracy: 89.82%; macro-F1: 88.95%

T

## Required software

1. Arduino IDE 2.x
2. **ESP32 by Espressif Systems 3.x**
3. Install **Chirale_TensorFlowLite** from Arduino Library Manager. Remove or
   temporarily rename older conflicting `Arduino_TensorFlowLite` and
   `TensorFlowLite_ESP32` library folders before compiling.

## Upload

1. Open `AuraSync_Showcase_Firmware.ino`.
2. Select **ESP32S3 Dev Module**, 8 MB flash, and **OPI PSRAM: Enabled**.
3. Check the INMP441 pins in `aurasync_config.h` and upload.
4. Open Serial Monitor at 115200 baud. Confirm `INT8 DS-CNN ready` appears.
5. Connect to Wi-Fi `AuraSync-Showcase` / `AuraSync2026`.
6. Open `http://192.168.4.1`.

See `UPLOAD_GUIDE.md` for detailed settings and troubleshooting.

## Spike behavior

A spike now produces one 350 ms dashboard blink. The detector must see 300 ms
without a spike candidate before it re-arms. Sustained high energy therefore
cannot hold `SPIKE` on-screen or repeatedly count the same physical event.

## Files

```text
AuraSync_Showcase_Firmware/
├── AuraSync_Showcase_Firmware.ino     Main firmware
├── aurasync_config.h                  Pins, Wi-Fi and thresholds
├── aurasync_dashboard.h               Embedded web dashboard
├── aurasync_model.h                   TensorFlow Lite Micro runtime
├── aurasync_model_data.h              Trained INT8 model bytes
├── aurasync_preprocessing_data.h       Exported Hann/Mel coefficients
├── aurasync_preprocessing_params.h     Model normalization and labels
├── HARDWARE_WIRING.md                  INMP441 wiring
├── UPLOAD_GUIDE.md                     Arduino upload instructions
└── README.md
```

The first seven files are required by the firmware build. The three Markdown
files are retained only so the submission can be wired and uploaded correctly.
The separate `.tflite` file is intentionally absent because the identical model
is already embedded in `aurasync_model_data.h`.

The model resolver registers only the five operators actually present in the
trained model: Conv2D, DepthwiseConv2D, Mean, FullyConnected and Softmax. It does
not depend on `all_ops_resolver.h`.

## Important limits

- Enable PSRAM: the TensorFlow Lite Micro arena is 512 KB.
- Physical dBA values require calibration against a reference sound-level meter.
- Model metrics measure the held-out dataset, not guaranteed field performance.
- Raw audio stays in volatile RAM; the firmware has no recording/upload endpoint.
