#pragma once

/**
 * @file open_wakeword_detector.h
 * @brief Three-model openWakeWord ONNX pipeline for wake word detection.
 *
 * Processes PCM audio through a melspectrogram → embedding → classifier chain.
 * Audio is accumulated in 1280-sample (80ms) chunks for the mel model, mel frames
 * are accumulated to 76 for the embedding model, and embedding features are
 * accumulated to 16 for the classifier.
 *
 * Fires wakeWordDetected when the classifier score exceeds the threshold,
 * with a ~200 ms pre-roll buffer and 500 ms false-wake suppression.
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
 *
 * Data flow: 1280 PCM samples → mel [1,1,5,32] → accumulate 76 mel frames →
 * embedding [1,76,32,1] → [1,1,1,96] → accumulate 16 features →
 * classifier [1,16,96] → [1,1] score.
 */
class OpenWakewordDetector : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the detector with injected ONNX sessions.
     * @param melSession Melspectrogram model session.
     * @param embeddingSession Embedding model session (input name: "input_1").
     * @param classifierSession Classifier session (input name: "x.1").
     * @param threshold Detection threshold in [0, 1]. Default 0.5.
     * @param parent QObject parent.
     */
    explicit OpenWakewordDetector(pairion::core::OnnxInferenceSession *melSession,
                                  pairion::core::OnnxInferenceSession *embeddingSession,
                                  pairion::core::OnnxInferenceSession *classifierSession,
                                  double threshold = 0.5, QObject *parent = nullptr);

  public slots:
    /**
     * @brief Process a 20 ms PCM frame through the wake word pipeline.
     * @param pcm20ms 640 bytes of 16-bit signed mono PCM at 16 kHz.
     */
    void processPcmFrame(const QByteArray &pcm20ms);

  signals:
    /**
     * @brief Emitted when the wake word is detected above threshold.
     * @param score Classifier confidence in [0, 1].
     * @param preRollBuffer ~200 ms of PCM audio preceding the wake trigger.
     */
    void wakeWordDetected(float score, const QByteArray &preRollBuffer);

  private:
    void processChunk(const std::vector<float> &samples1280);
    void runClassifier();

    pairion::core::OnnxInferenceSession *m_melSession;
    pairion::core::OnnxInferenceSession *m_embeddingSession;
    pairion::core::OnnxInferenceSession *m_classifierSession;
    double m_threshold;

    // PCM accumulator for 1280-sample mel chunks (2560 bytes)
    QByteArray m_pcmAccumulator;

    // Mel frame accumulator (need 76 frames of 32 floats each)
    std::deque<std::vector<float>> m_melFrames;

    // Embedding feature accumulator (need 16 features of 96 floats each)
    std::deque<std::vector<float>> m_embFeatures;

    // Pre-roll buffer
    QByteArray m_preRollBuffer;
    QElapsedTimer m_suppressionTimer;
    bool m_suppressionActive = false;

    static constexpr int kMelChunkSamples = 1280; ///< 80 ms at 16 kHz
    static constexpr int kMelChunkBytes = 2560;   ///< 1280 * 2 bytes
    static constexpr int kMelFramesNeeded = 76;   ///< Frames for embedding
    static constexpr int kMelFrameWidth = 32;     ///< Floats per mel frame
    static constexpr int kEmbFeaturesNeeded = 16; ///< Features for classifier
    static constexpr int kEmbFeatureSize = 96;    ///< Floats per embedding
    static constexpr int kPreRollBytes = 6400;    ///< ~200 ms at 16 kHz 16-bit mono
    static constexpr int kSuppressionMs = 500;    ///< False-wake suppression window
};

} // namespace pairion::wake
