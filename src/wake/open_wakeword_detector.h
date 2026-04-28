#pragma once

/**
 * @file open_wakeword_detector.h
 * @brief Three-model openWakeWord ONNX pipeline for wake word detection.
 *
 * Processes PCM audio through a melspectrogram → embedding → classifier chain
 * matching the openWakeWord Python library's data flow. Uses fixed 1760-sample
 * sliding window (1280 chunk + 480 context) for mel input to eliminate numerical
 * drift vs Python reference. Mel rows feed rolling buffer for embedding.
 */

#include "../core/onnx_session.h"

#include <deque>
#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <vector>

namespace pairion::wake {

/**
 * @brief Wake word detector using the openWakeWord three-model ONNX pipeline.
 */
class OpenWakewordDetector : public QObject {
    Q_OBJECT
  public:
    explicit OpenWakewordDetector(pairion::core::OnnxInferenceSession *melSession,
                                  pairion::core::OnnxInferenceSession *embeddingSession,
                                  pairion::core::OnnxInferenceSession *classifierSession,
                                  double threshold = 0.5, QObject *parent = nullptr);

  public slots:
    /// @brief Pre-warm the pipeline with silence so the classifier is ready immediately.
    void warmup();
    /// @brief Process a 20 ms PCM frame (640 bytes, 16-bit mono 16 kHz).
    void processPcmFrame(const QByteArray &pcm20ms);

  public:
    /**
     * @brief Return the current pre-roll PCM buffer contents.
     *
     * The pre-roll buffer holds the most recent ~200 ms of raw PCM audio.
     * AudioSessionOrchestrator reads this on confirmed barge-in to send
     * the beginning of the user's interruption to the server.
     * @return Current pre-roll buffer (may be empty before any audio is processed).
     */
    QByteArray preRollBuffer() const { return m_preRollBuffer; }

    /**
     * @brief Inject pre-roll data directly — test use only.
     *
     * Replaces the internal pre-roll buffer with @p data so that unit tests
     * can exercise the barge-in pre-roll encoding path without running the
     * full ONNX pipeline.
     * @param data Raw PCM bytes to install as the pre-roll buffer.
     */
    void setPreRollBufferForTest(const QByteArray &data) { m_preRollBuffer = data; }

  signals:
    /// @brief Emitted when the wake word is detected above threshold.
    void wakeWordDetected(float score, const QByteArray &preRollBuffer);

  private:
    void processAccumulatedAudio();
    void runClassifier();

    pairion::core::OnnxInferenceSession *m_melSession;
    pairion::core::OnnxInferenceSession *m_embeddingSession;
    pairion::core::OnnxInferenceSession *m_classifierSession;
    double m_threshold;

    // Raw audio buffer (float32, normalized) — sliding window for mel model
    std::vector<float> m_rawAudioBuffer;
    int m_accumulatedSamples = 0;

    // Rolling mel spectrogram buffer (each entry is one row of 32 floats)
    std::deque<std::vector<float>> m_melBuffer;

    // Rolling embedding feature buffer (each entry is 96 floats)
    std::deque<std::vector<float>> m_embFeatures;

    // Pre-roll buffer
    QByteArray m_preRollBuffer;
    QElapsedTimer m_suppressionTimer;
    bool m_suppressionActive = false;

    static constexpr int kMelChunkSamples = 1280;      ///< 80 ms at 16 kHz
    static constexpr int kMelContextSamples = 480;     ///< 160*3 overlap context
    static constexpr int kMelRowsPerChunk = 8;         ///< New mel rows per 1280-sample chunk
    static constexpr int kMelFrameWidth = 32;          ///< Floats per mel row
    static constexpr int kMelFramesNeeded = 76;        ///< Mel rows for embedding
    static constexpr int kMelBufferMaxLen = 200;       ///< Max mel rows to keep
    static constexpr int kEmbFeaturesNeeded = 16;      ///< Features for classifier
    static constexpr int kEmbFeatureSize = 96;         ///< Floats per embedding
    static constexpr int kFeatureBufferMaxLen = 120;   ///< Max features to keep
    static constexpr int kMaxRawBufferSamples = 32000; ///< ~2 seconds of raw audio
    static constexpr int kPreRollBytes = 6400;         ///< ~200 ms pre-roll
    static constexpr int kSuppressionMs = 500;         ///< False-wake suppression
};

} // namespace pairion::wake
