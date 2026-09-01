/*
  AuraSync Showcase Firmware v2
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "driver/i2s_std.h"

#include "aurasync_config.h"
#include "aurasync_model.h"
#include "aurasync_preprocessing_data.h"
#include "aurasync_dashboard.h"

// ----------------------------- Audio constants -----------------------------
constexpr int SAMPLE_RATE = 16000;
constexpr int DSP_BLOCK_MS = 100;
constexpr int DSP_BLOCK_SAMPLES = 1600;
constexpr int I2S_READ_SAMPLES = 400;
constexpr int CNN_WINDOW_SAMPLES = 16000;
constexpr int CNN_FFT_SIZE = 512;
constexpr int DSP_FFT_SIZE = 256;
constexpr float HIGH_PASS_ALPHA = 0.995f;

// ----------------------------- Runtime types -------------------------------
enum AuraState : uint8_t {
  STATE_MIC_ERROR,
  STATE_SMOOTH,
  STATE_PRE_WARNING,
  STATE_WARNING,
  STATE_LOUD,
  STATE_RECOVERY,
  STATE_SPIKE
};

struct DspResult {
  float laeq1sDba;
  float aWeightedDbfs;
  float peakDb;
  float rmsDb;
  float baselineDba;
  float snrDb;
  float crestDb;
  float speechBandRatio;
  float spectralFlux;
  float impulsiveness;
  float micQuality;
  float clipPercent;
  float zeroPercent;
};

struct TelemetrySnapshot {
  uint32_t sequence;
  uint32_t uptimeSeconds;
  AuraState state;
  float score;
  float persistence;
  DspResult dsp;
  AuraSyncPrediction cnn;
  float cnnInferenceMs;
  bool micOk;
  bool simulation;
  uint32_t i2sErrors;
  uint32_t eventCount;
  uint32_t spikeCount;
};

struct BiquadState {
  float z1;
  float z2;
};

// ----------------------------- Global services -----------------------------
WebServer server(80);
Preferences preferences;
i2s_chan_handle_t i2sRx = nullptr;
TaskHandle_t audioTaskHandle = nullptr;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;

TelemetrySnapshot snapshot = {};
volatile bool simulationMode = false;
bool microphoneReady = false;
float calibrationOffsetDb = AURASYNC_DEFAULT_CAL_OFFSET_DB;

// ------------------------------ Audio memory -------------------------------
int32_t i2sRaw[I2S_READ_SAMPLES];
int16_t audioBlock[DSP_BLOCK_SAMPLES];
int16_t cnnRing[CNN_WINDOW_SAMPLES];
int16_t cnnOrdered[CNN_WINDOW_SAMPLES];
int cnnWriteIndex = 0;
int cnnSamplesCollected = 0;

float dspFftReal[DSP_FFT_SIZE];
float dspFftImag[DSP_FFT_SIZE];
float dspPreviousSpectrum[(DSP_FFT_SIZE / 2) + 1] = {};
float cnnFftReal[CNN_FFT_SIZE];
float cnnFftImag[CNN_FFT_SIZE];
float cnnLogMel[AURASYNC_TIME_FRAMES][AURASYNC_N_MELS];

float weightedEnergyRing[10] = {};
float weightedEnergySum = 0.0f;
int weightedEnergyIndex = 0;
int weightedEnergyCount = 0;

float highPassLastX = 0.0f;
float highPassLastY = 0.0f;
BiquadState aWeightState[3] = {};

// A-weighting filter at 16 kHz, generated from the IEC analogue pole/zero model.
// Each row is b0, b1, b2, a1, a2 for a transposed direct-form-II biquad.
const float A_WEIGHT_SOS[3][5] = {
  {0.53148983f, 1.06297966f, 0.53148983f, 0.82156382f, 0.16874178f},
  {1.00000000f, -2.00000000f, 1.00000000f, -1.70550963f, 0.71598758f},
  {1.00000000f, -2.00000000f, 1.00000000f, -1.98388676f, 0.98395167f}
};

// -------------------------- Decision/state memory --------------------------
AuraState stableState = STATE_SMOOTH;
AuraState displayState = STATE_SMOOTH;
AuraSyncPrediction currentPrediction = {0.34f, 0.33f, 0.33f, 0.34f, 0};
float currentInferenceMs = 0.0f;
float adaptiveBaselineDba = 45.0f;
bool baselineInitialized = false;
float persistenceValue = 0.0f;
uint32_t spikeVisibleUntilMs = 0;
uint32_t spikeClearStartedMs = 0;
bool spikeArmed = true;
bool modelReady = false;
uint32_t eventCount = 0;
uint32_t spikeCount = 0;
uint32_t i2sErrorCount = 0;
int micBadMs = 0;
int loudMs = 0;
int warningMs = 0;
int preWarningMs = 0;
int recoveryMs = 0;

// ------------------------------- Utilities ---------------------------------
float clampFloat(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

int16_t clampInt16(int32_t value) {
  if (value > 32767) return 32767;
  if (value < -32768) return -32768;
  return static_cast<int16_t>(value);
}

float powerToDb(float power) {
  return 10.0f * log10f((power > 1e-12f) ? power : 1e-12f);
}

float amplitudeToDb(float amplitude) {
  return 20.0f * log10f((amplitude > 1e-9f) ? amplitude : 1e-9f);
}

const char* stateName(AuraState state) {
  switch (state) {
    case STATE_MIC_ERROR: return "MIC_ERROR";
    case STATE_SMOOTH: return "SMOOTH";
    case STATE_PRE_WARNING: return "PRE_WARNING";
    case STATE_WARNING: return "WARNING";
    case STATE_LOUD: return "LOUD";
    case STATE_RECOVERY: return "RECOVERY";
    case STATE_SPIKE: return "SPIKE";
    default: return "UNKNOWN";
  }
}

void fastFft(float* real, float* imag, int size) {
  int j = 0;
  for (int i = 1; i < size; ++i) {
    int bit = size >> 1;
    while (j & bit) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;
    if (i < j) {
      float temporary = real[i]; real[i] = real[j]; real[j] = temporary;
      temporary = imag[i]; imag[i] = imag[j]; imag[j] = temporary;
    }
  }

  for (int length = 2; length <= size; length <<= 1) {
    const float angleStep = -2.0f * PI / length;
    for (int start = 0; start < size; start += length) {
      for (int offset = 0; offset < length / 2; ++offset) {
        const float angle = angleStep * offset;
        const float cosine = cosf(angle);
        const float sine = sinf(angle);
        const int evenIndex = start + offset;
        const int oddIndex = evenIndex + length / 2;
        const float oddReal = real[oddIndex] * cosine - imag[oddIndex] * sine;
        const float oddImag = real[oddIndex] * sine + imag[oddIndex] * cosine;
        real[oddIndex] = real[evenIndex] - oddReal;
        imag[oddIndex] = imag[evenIndex] - oddImag;
        real[evenIndex] += oddReal;
        imag[evenIndex] += oddImag;
      }
    }
  }
}

float applyAWeighting(float input) {
  float value = input;
  for (int stage = 0; stage < 3; ++stage) {
    const float output = A_WEIGHT_SOS[stage][0] * value + aWeightState[stage].z1;
    aWeightState[stage].z1 = A_WEIGHT_SOS[stage][1] * value
                            - A_WEIGHT_SOS[stage][3] * output
                            + aWeightState[stage].z2;
    aWeightState[stage].z2 = A_WEIGHT_SOS[stage][2] * value
                            - A_WEIGHT_SOS[stage][4] * output;
    value = output;
  }
  return value;
}

void resetSignalMemory() {
  highPassLastX = 0.0f;
  highPassLastY = 0.0f;
  memset(aWeightState, 0, sizeof(aWeightState));
  memset(weightedEnergyRing, 0, sizeof(weightedEnergyRing));
  memset(dspPreviousSpectrum, 0, sizeof(dspPreviousSpectrum));
  weightedEnergySum = 0.0f;
  weightedEnergyIndex = 0;
  weightedEnergyCount = 0;
  cnnWriteIndex = 0;
  cnnSamplesCollected = 0;
}

void resetDecisionMemory() {
  stableState = STATE_SMOOTH;
  displayState = STATE_SMOOTH;
  persistenceValue = 0.0f;
  micBadMs = loudMs = warningMs = preWarningMs = recoveryMs = 0;
  spikeVisibleUntilMs = 0;
  spikeClearStartedMs = 0;
  spikeArmed = true;
}

