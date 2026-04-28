#pragma once

/**
 * @file silero_vad.h
 * @brief Silero VAD ONNX model wrapper with speech start/end state machine.
 *
 * Processes 20 ms PCM frames, accumulates to 512 samples for the model's
 * expected input size, runs inference with recurrent state tensor, and emits
 * speechStarted/speechEnded signals based on configurable thresholds.
 *
 * Silero VAD v5 input signature:
 *   input: [1, 512] float32
 *   state: [2, 1, 128] float32
 *   sr: scalar int64 (sample rate, 16000)
 * Output:
 *   output: [1, 1] float32 (speech probability)
 *   stateN: [2, 1, 128] float32 (updated recurrent state)
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
 */
class SileroVad : public QObject {
    Q_OBJECT
  public:
    explicit SileroVad(pairion::core::OnnxInferenceSession *session, double threshold = 0.5,
                       int silenceEndMs = 800, QObject *parent = nullptr);

  public slots:
    void processPcmFrame(const QByteArray &pcm20ms);
    void reset();

    /**
     * @brief Switch the speech probability threshold at runtime.
     *
     * Used by AudioSessionOrchestrator to raise the threshold during TTS playback
     * (barge-in mode) and restore it when playback ends.  Caller is responsible
     * for calling reset() if the VAD state machine should be cleared simultaneously.
     * @param threshold Speech probability [0, 1] above which speech is considered active.
     */
    void setThreshold(double threshold);

    /**
     * @brief Return the current speech probability threshold.
     * @return Current threshold value.
     */
    double threshold() const;

  signals:
    void speechStarted();
    void speechEnded();

  private:
    void runInference(const std::vector<float> &samples);

    pairion::core::OnnxInferenceSession *m_session;
    double m_threshold;
    int m_silenceEndMs;

    enum class VadState { Idle, Speaking, Trailing };
    VadState m_state = VadState::Idle;
    QElapsedTimer m_silenceTimer;

    // Recurrent state tensor: [2, 1, 128] float32
    std::vector<float> m_recurrentState;

    QByteArray m_accumulator;
    static constexpr int kInferenceSamples = 512;
    static constexpr int kInferenceBytes = 1024; ///< 512 samples * 2 bytes
    static constexpr int kStateSize = 256;       ///< 2 * 1 * 128 = 256 floats
};

} // namespace pairion::vad
