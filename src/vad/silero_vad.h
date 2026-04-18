#pragma once

/**
 * @file silero_vad.h
 * @brief Silero VAD ONNX model wrapper with speech start/end state machine.
 *
 * Processes 20 ms PCM frames, accumulates to the model's expected input size,
 * runs inference with recurrent state (h, c tensors), and emits speechStarted/
 * speechEnded signals based on configurable thresholds.
 */

#include "../core/onnx_session.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <vector>

namespace pairion::vad {

/**
 * @brief Voice activity detector using the Silero VAD ONNX model.
 *
 * State machine: Idle → Speaking → Trailing → Idle.
 * Emits speechStarted() when speech probability exceeds threshold,
 * speechEnded() after sustained silence exceeding the configurable duration.
 */
class SileroVad : public QObject {
    Q_OBJECT
  public:
    /**
     * @brief Construct the VAD with an injected ONNX session.
     * @param session Silero VAD model session.
     * @param threshold Speech probability threshold in [0, 1]. Default 0.5.
     * @param silenceEndMs Milliseconds of silence before emitting speechEnded. Default 800.
     * @param parent QObject parent.
     */
    explicit SileroVad(pairion::core::OnnxInferenceSession *session, double threshold = 0.5,
                       int silenceEndMs = 800, QObject *parent = nullptr);

  public slots:
    /**
     * @brief Process a 20 ms PCM frame.
     * @param pcm20ms 640 bytes of 16-bit signed mono PCM at 16 kHz.
     */
    void processPcmFrame(const QByteArray &pcm20ms);

    /**
     * @brief Reset the VAD state machine and recurrent state to initial values.
     */
    void reset();

  signals:
    /// Emitted on transition from silence to speech.
    void speechStarted();
    /// Emitted after sustained silence exceeding the configured threshold.
    void speechEnded();

  private:
    void runInference(const std::vector<float> &samples);

    pairion::core::OnnxInferenceSession *m_session;
    double m_threshold;
    int m_silenceEndMs;

    enum class VadState { Idle, Speaking, Trailing };
    VadState m_state = VadState::Idle;
    QElapsedTimer m_silenceTimer;

    // Recurrent state for Silero VAD (h and c tensors, shape [2, 1, 64])
    std::vector<float> m_stateH;
    std::vector<float> m_stateC;

    // Accumulator for collecting enough samples for inference
    QByteArray m_accumulator;
    static constexpr int kInferenceSamples = 512; ///< Silero expects 512 samples per inference
    static constexpr int kInferenceBytes = 1024;  ///< 512 samples * 2 bytes
    static constexpr int kStateSize = 128;        ///< 2 * 1 * 64 = 128 floats
};

} // namespace pairion::vad
