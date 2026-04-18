# ONNX Model Assets

Models are downloaded on first run and cached at `~/Library/Application Support/Pairion/models/openwakeword/`.

## openWakeWord Models (v0.5.1)

Three models form the wake-word detection pipeline. All from the [openWakeWord](https://github.com/dscripka/openWakeWord) project.

### melspectrogram.onnx

- **Purpose:** Converts raw 16kHz audio into melspectrogram features
- **URL:** `https://github.com/dscripka/openWakeWord/releases/download/v0.5.1/melspectrogram.onnx`
- **SHA-256:** `ba2b0e0f8b7b875369a2c89cb13360ff53bac436f2895cced9f479fa65eb176f`
- **Size:** 1,087,958 bytes
- **License:** Apache-2.0
- **Version:** v0.5.1

### embedding_model.onnx

- **Purpose:** Shared feature-extraction backbone (audio → embeddings)
- **URL:** `https://github.com/dscripka/openWakeWord/releases/download/v0.5.1/embedding_model.onnx`
- **SHA-256:** `70d164290c1d095d1d4ee149bc5e00543250a7316b59f31d056cff7bd3075c1f`
- **Size:** 1,326,578 bytes
- **License:** Apache-2.0
- **Version:** v0.5.1

### hey_jarvis_v0.1.onnx

- **Purpose:** Wake-word-specific scorer for the phrase "Hey Jarvis"
- **URL:** `https://github.com/dscripka/openWakeWord/releases/download/v0.5.1/hey_jarvis_v0.1.onnx`
- **SHA-256:** `94a13cfe60075b132f6a472e7e462e8123ee70861bc3fb58434a73712ee0d2cb`
- **Size:** 1,271,370 bytes
- **License:** CC BY-NC-SA 4.0 (per amended Charter §13.3)
- **Version:** v0.5.1
- **Note:** M1 placeholder model. The intended brand wake phrase is "Hey Pairion"; a custom model will be trained in a later milestone.

## Silero VAD

### silero_vad.onnx

- **Purpose:** Voice activity detection (speech/silence classification)
- **URL:** `https://github.com/dscripka/openWakeWord/releases/download/v0.5.1/silero_vad.onnx`
- **SHA-256:** `a35ebf52fd3ce5f1469b2a36158dba761bc47b973ea3382b3186ca15b1f5af28`
- **Size:** 1,807,522 bytes
- **License:** MIT (Silero project)
- **Version:** v0.5.1 (bundled in openWakeWord release)
- **Upstream:** [snakers4/silero-vad](https://github.com/snakers4/silero-vad)
