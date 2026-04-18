#pragma once

/**
 * @file open_wakeword_detector.h
 * @brief Three-model openWakeWord ONNX pipeline for wake word detection.
 *
 * Processes 20 ms PCM frames through a melspectrogram → embedding → classifier
 * chain. Fires wakeWordDetected when the classifier score exceeds the threshold,
 * with a ~200 ms pre-roll buffer and 500 ms false-wake suppression.
 */

#include "../core/onnx_session.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>

namespace pairion::wake {

/**
 * @brief Wake word detector using the openWakeWord three-model ONNX pipeline.
 *
 * Receives PCM frames, runs mel → embedding → classifier inference, and emits
 * a signal when the wake phrase "Hey Jarvis" is detected above threshold.
 *
 * Sessions are injected via constructor for testability.
 */
class OpenWakewordDetector : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the detector with injected ONNX sessions.
     * @param melSession Melspectrogram model session.
     * @param embeddingSession Embedding model session.
     * @param classifierSession Hey Jarvis classifier session.
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
    pairion::core::OnnxInferenceSession *m_melSession;
    pairion::core::OnnxInferenceSession *m_embeddingSession;
    pairion::core::OnnxInferenceSession *m_classifierSession;
    double m_threshold;
    QByteArray m_preRollBuffer;
    QElapsedTimer m_suppressionTimer;
    bool m_suppressionActive = false;

    static constexpr int kPreRollBytes = 6400; ///< ~200 ms at 16 kHz 16-bit mono
    static constexpr int kSuppressionMs = 500; ///< False-wake suppression window
};

} // namespace pairion::wake
