/**
 * @file silero_vad.cpp
 * @brief Implementation of the Silero VAD with recurrent state management.
 */

#include "silero_vad.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcVad, "pairion.vad")

namespace pairion::vad {

SileroVad::SileroVad(pairion::core::OnnxInferenceSession *session, double threshold,
                     int silenceEndMs, QObject *parent)
    : QObject(parent), m_session(session), m_threshold(threshold), m_silenceEndMs(silenceEndMs),
      m_stateH(kStateSize, 0.0f), m_stateC(kStateSize, 0.0f) {}

void SileroVad::processPcmFrame(const QByteArray &pcm20ms) {
    m_accumulator.append(pcm20ms);

    while (m_accumulator.size() >= kInferenceBytes) {
        QByteArray chunk = m_accumulator.left(kInferenceBytes);
        m_accumulator.remove(0, kInferenceBytes);

        // Convert int16 to float32
        const auto *pcm = reinterpret_cast<const int16_t *>(chunk.constData());
        std::vector<float> samples(kInferenceSamples);
        for (int i = 0; i < kInferenceSamples; ++i) {
            samples[i] = static_cast<float>(pcm[i]) / 32768.0f;
        }

        runInference(samples);
    }
}

void SileroVad::reset() {
    m_state = VadState::Idle;
    m_accumulator.clear();
    std::fill(m_stateH.begin(), m_stateH.end(), 0.0f);
    std::fill(m_stateC.begin(), m_stateC.end(), 0.0f);
    qCInfo(lcVad) << "VAD state reset";
}

void SileroVad::runInference(const std::vector<float> &samples) {
    // Build inputs: audio, h state, c state, sample rate
    pairion::core::OnnxTensor audioInput{"input", samples, {1, kInferenceSamples}};
    pairion::core::OnnxTensor hInput{"h", m_stateH, {2, 1, 64}};
    pairion::core::OnnxTensor cInput{"c", m_stateC, {2, 1, 64}};

    auto outputs = m_session->run({audioInput, hInput, cInput}, {"output", "hn", "cn"});

    // Update recurrent state
    if (outputs.size() >= 3) {
        m_stateH = outputs[1].data;
        m_stateC = outputs[2].data;
    }

    float speechProb = outputs[0].data.empty() ? 0.0f : outputs[0].data[0];

    // State machine transitions
    switch (m_state) {
    case VadState::Idle:
        if (speechProb >= m_threshold) {
            m_state = VadState::Speaking;
            qCInfo(lcVad) << "Speech started, prob:" << speechProb;
            emit speechStarted();
        }
        break;

    case VadState::Speaking:
        if (speechProb < m_threshold) {
            m_state = VadState::Trailing;
            m_silenceTimer.start();
            qCDebug(lcVad) << "Trailing silence started";
        }
        break;

    case VadState::Trailing:
        if (speechProb >= m_threshold) {
            m_state = VadState::Speaking;
            qCDebug(lcVad) << "Speech resumed during trailing";
        } else if (m_silenceTimer.elapsed() >= m_silenceEndMs) {
            m_state = VadState::Idle;
            qCInfo(lcVad) << "Speech ended after" << m_silenceTimer.elapsed() << "ms silence";
            emit speechEnded();
        }
        break;
    }
}

} // namespace pairion::vad
