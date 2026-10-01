#!/usr/bin/env python3
"""Offline integrity checks for the submission package (requires NumPy)."""

import hashlib
import re
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]


def floats_from_header(text: str, symbol: str) -> np.ndarray:
    body = re.search(rf"{symbol}\[\d+\]\s*=\s*\{{(.*?)\}};", text, re.S)
    assert body, f"missing {symbol}"
    return np.asarray(
        [float(item.rstrip("f")) for item in body.group(1).split(",") if item.strip()],
        dtype=np.float32,
    )


def test_model_header_matches_tflite():
    model = (ROOT / "model/aurasync_3class_ds_cnn_full_int8.tflite").read_bytes()
    header = (ROOT / "aurasync_model_data.h").read_text()
    body = re.search(r"g_aurasync_3class_model\[\]\s*=\s*\{(.*?)\};", header, re.S)
    assert body
    embedded = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", body.group(1)))
    assert embedded == model
    assert len(model) == 77384
    assert hashlib.sha256(model).hexdigest() == (
        "5f687e9590ba40d5f1a6c8711aafd89d6d543f240bad4502263a936dfecb76ee"
    )


def test_preprocessing_array_dimensions_and_ranges():
    text = (ROOT / "aurasync_preprocessing_data.h").read_text()
    window = floats_from_header(text, "AURASYNC_HANN_WINDOW")
    filters = floats_from_header(text, "AURASYNC_MEL_FILTERBANK")
    assert window.shape == (512,)
    assert filters.shape == (64 * 257,)
    assert np.isclose(window.min(), 0.0) and np.isclose(window.max(), 1.0)
    assert filters.min() >= 0.0 and filters.max() > 0.02


def test_no_dummy_classifier_references():
    source = (ROOT / "AuraSync_Showcase_Firmware.ino").read_text()
    model = (ROOT / "aurasync_model.h").read_text()
    combined = source + model
    assert "AuraSyncTinyCNN" not in combined
    assert "AS_CNN_FRAMES" not in combined
    assert "runTinyCnn" not in combined
    assert "AuraSyncModel::predict" in source
    assert "g_aurasync_3class_model" in model


if __name__ == "__main__":
    test_model_header_matches_tflite()
    test_preprocessing_array_dimensions_and_ranges()
    test_no_dummy_classifier_references()
    print("PASS: model bytes, preprocessing arrays, and trained inference path verified")
