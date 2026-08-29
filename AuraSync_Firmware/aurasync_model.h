#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>

#if __has_include(<Chirale_TensorFlowLite.h>)
// Preferred Arduino library: current TFLite Micro tree with mutable resolver.
#include <Chirale_TensorFlowLite.h>
#elif __has_include("tensorflow/lite/micro/micro_interpreter.h")
// Also supports a complete TFLite Micro tree exposed by the ESP32 core.
#elif __has_include(<TensorFlowLite_ESP32.h>)
#include <TensorFlowLite_ESP32.h>
#else
#error "TensorFlow Lite Micro is missing. Use Arduino-ESP32 3.x or install Chirale_TensorFlowLite."
#endif

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "aurasync_model_data.h"
#include "aurasync_preprocessing_params.h"

#define AURASYNC_MODEL_VERSION "ds-cnn-int8-v2.0.0"
#define AURASYNC_TENSOR_ARENA_BYTES (512 * 1024)

struct AuraSyncPrediction {
  float stationaryBackground;
  float speechDominant;
  float nonSpeechActivity;
  float confidence;
  uint8_t classIndex;
};

class AuraSyncModel {
 public:
  static bool begin() {
    if (interpreter_ != nullptr) return true;

    model_ = tflite::GetModel(g_aurasync_3class_model);
    if (model_ == nullptr || model_->version() != TFLITE_SCHEMA_VERSION) {
      Serial.println("CNN initialization failed: incompatible TFLite schema.");
      return false;
    }

    tensorArena_ = static_cast<uint8_t*>(heap_caps_malloc(
      AURASYNC_TENSOR_ARENA_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    ));
    if (tensorArena_ == nullptr) {
      tensorArena_ = static_cast<uint8_t*>(heap_caps_malloc(
        AURASYNC_TENSOR_ARENA_BYTES, MALLOC_CAP_8BIT
      ));
    }
    if (tensorArena_ == nullptr) {
      Serial.println("CNN initialization failed: tensor arena allocation failed. Enable OPI PSRAM.");
      return false;
    }

    // The exported DS-CNN contains exactly five operator types. Registering
    // only these avoids all_ops_resolver.h, which is absent in some Arduino
    // TensorFlow Lite Micro packages, and keeps the firmware smaller.
    static tflite::MicroMutableOpResolver<5> resolver;
    if (resolver.AddConv2D() != kTfLiteOk
        || resolver.AddDepthwiseConv2D() != kTfLiteOk
        || resolver.AddMean() != kTfLiteOk
        || resolver.AddFullyConnected() != kTfLiteOk
        || resolver.AddSoftmax() != kTfLiteOk) {
      Serial.println("CNN initialization failed: operator registration failed.");
      return false;
    }
    static tflite::MicroInterpreter interpreter(
      model_, resolver, tensorArena_, AURASYNC_TENSOR_ARENA_BYTES
    );
    interpreter_ = &interpreter;
    if (interpreter_->AllocateTensors() != kTfLiteOk) {
      Serial.println("CNN initialization failed: AllocateTensors returned an error.");
      interpreter_ = nullptr;
      return false;
    }

    input_ = interpreter_->input(0);
    output_ = interpreter_->output(0);
    if (!validTensorContract()) {
      Serial.println("CNN initialization failed: deployed tensor contract does not match preprocessing.");
      interpreter_ = nullptr;
      return false;
    }
    Serial.printf("INT8 DS-CNN ready; arena used: %u / %u bytes.\n",
                  static_cast<unsigned>(interpreter_->arena_used_bytes()),
                  static_cast<unsigned>(AURASYNC_TENSOR_ARENA_BYTES));
    return true;
  }

  static bool ready() { return interpreter_ != nullptr; }

  static const char* className(uint8_t index) {
    return (index < AURASYNC_CLASS_COUNT) ? AURASYNC_LABELS[index] : "unknown";
  }

  static bool predict(const float logMel[AURASYNC_TIME_FRAMES][AURASYNC_N_MELS],
                      AuraSyncPrediction& result) {
    if (!ready()) return false;

    const float scale = input_->params.scale;
    const int zeroPoint = input_->params.zero_point;
    int index = 0;
    for (int frame = 0; frame < AURASYNC_TIME_FRAMES; ++frame) {
      for (int mel = 0; mel < AURASYNC_N_MELS; ++mel) {
        int quantized = static_cast<int>(roundf(logMel[frame][mel] / scale)) + zeroPoint;
        if (quantized < -128) quantized = -128;
        if (quantized > 127) quantized = 127;
        input_->data.int8[index++] = static_cast<int8_t>(quantized);
      }
    }

    if (interpreter_->Invoke() != kTfLiteOk) {
      Serial.println("CNN inference failed: Invoke returned an error.");
      return false;
    }

    float probabilities[AURASYNC_CLASS_COUNT];
    float sum = 0.0f;
    for (int i = 0; i < AURASYNC_CLASS_COUNT; ++i) {
      probabilities[i] = (static_cast<int>(output_->data.int8[i])
                          - output_->params.zero_point) * output_->params.scale;
      if (probabilities[i] < 0.0f) probabilities[i] = 0.0f;
      sum += probabilities[i];
    }
    if (sum <= 1e-6f) return false;
    for (float& probability : probabilities) probability /= sum;

    uint8_t best = 0;
    if (probabilities[1] > probabilities[best]) best = 1;
    if (probabilities[2] > probabilities[best]) best = 2;
    result = {probabilities[0], probabilities[1], probabilities[2],
              probabilities[best], best};
    return true;
  }

 private:
  static bool validTensorContract() {
    if (input_ == nullptr || output_ == nullptr) return false;
    if (input_->type != kTfLiteInt8 || output_->type != kTfLiteInt8) return false;
    if (input_->bytes != AURASYNC_TIME_FRAMES * AURASYNC_N_MELS) return false;
    if (output_->bytes != AURASYNC_CLASS_COUNT) return false;
    if (input_->params.scale <= 0.0f || output_->params.scale <= 0.0f) return false;
    if (fabsf(input_->params.scale - AURASYNC_INPUT_SCALE) > 1e-7f
        || input_->params.zero_point != AURASYNC_INPUT_ZERO_POINT) return false;
    if (fabsf(output_->params.scale - AURASYNC_OUTPUT_SCALE) > 1e-7f
        || output_->params.zero_point != AURASYNC_OUTPUT_ZERO_POINT) return false;
    return true;
  }

  inline static const tflite::Model* model_ = nullptr;
  inline static tflite::MicroInterpreter* interpreter_ = nullptr;
  inline static TfLiteTensor* input_ = nullptr;
  inline static TfLiteTensor* output_ = nullptr;
  inline static uint8_t* tensorArena_ = nullptr;
};
