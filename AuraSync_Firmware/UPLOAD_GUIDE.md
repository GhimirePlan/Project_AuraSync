# Arduino IDE Upload Guide

## Install

Install these through Arduino IDE:

- Boards Manager: **ESP32 by Espressif Systems**, version 3.x
- Library Manager: install **Chirale_TensorFlowLite**.

Avoid having several TensorFlow Lite Arduino libraries installed simultaneously.
In `Documents/Arduino/libraries`, remove or rename old folders such as
`Arduino_TensorFlowLite` and `TensorFlowLite_ESP32`, restart Arduino IDE, and
compile again. Multiple versions can cause Arduino to select incomplete headers.

## Board options

| Option | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| CPU Frequency | 240 MHz |
| Flash Size | 8 MB |
| PSRAM | OPI PSRAM / Enabled |
| Upload speed | 921600, or 460800 if unstable |

PSRAM is required for the 512 KB TensorFlow Lite Micro tensor arena. Do not use
the old instruction that said PSRAM was optional.

## Upload and verify

1. Keep the `.ino` and all root `.h` files in the same folder.
2. Open `AuraSync_Showcase_Firmware.ino`, choose the board/port, and upload.
3. Open Serial Monitor at 115200 baud.
4. Verify the startup contains `INT8 DS-CNN ready`. If it does not, CNN output
   is unavailable; the dashboard and DSP intentionally continue running.
5. Connect to `AuraSync-Showcase` with password `AuraSync2026`.
6. Open `http://192.168.4.1`.

## Common problems

- **Missing `micro_mutable_op_resolver.h`:** remove conflicting old TensorFlow
  Lite libraries, install/update `Chirale_TensorFlowLite`, and restart Arduino IDE.
- **Tensor arena allocation failed:** enable OPI PSRAM and use an N8R8 board.
- **Tensor contract mismatch:** do not replace only the `.tflite`; regenerate
  `aurasync_model_data.h` and preprocessing exports together.
- **Continuous MIC_ERROR:** check INMP441 wiring and L/R selection.
- **Clipping:** increase `AURASYNC_MIC_SHIFT_BITS`, then recalibrate.
- **Weak signal:** carefully reduce `AURASYNC_MIC_SHIFT_BITS`, then recalibrate.
- **Upload fails:** use a data cable, hold BOOT as upload starts, or reduce speed.

## Calibration

Place a reference meter next to the INMP441, use a stable sound, enter the meter
reading in the dashboard, and press **Calibrate dBA**. One point is acceptable
for a showcase; research reporting needs multi-level validation.
