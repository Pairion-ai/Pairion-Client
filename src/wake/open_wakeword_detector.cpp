/**
 * @file open_wakeword_detector.cpp
 * @brief Implementation of the openWakeWord three-model detector.
 */

#include "open_wakeword_detector.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWake, "pairion.wake")

namespace pairion::wake {

OpenWakewordDetector::OpenWakewordDetector(pairion::core::OnnxInferenceSession *melSession,
                                           pairion::core::OnnxInferenceSession *embeddingSession,
                                           pairion::core::OnnxInferenceSession *classifierSession,
                                           double threshold, QObject *parent)
    : QObject(parent), m_melSession(melSession), m_embeddingSession(embeddingSession),
      m_classifierSession(classifierSession), m_threshold(threshold) {}

void OpenWakewordDetector::processPcmFrame(const QByteArray &pcm20ms) {
    // Maintain pre-roll buffer (rolling window of ~200 ms)
    m_preRollBuffer.append(pcm20ms);
    if (m_preRollBuffer.size() > kPreRollBytes) {
        m_preRollBuffer = m_preRollBuffer.right(kPreRollBytes);
    }

    // Convert PCM int16 to float32 for ONNX input
    int sampleCount = pcm20ms.size() / 2;
    const auto *pcmData = reinterpret_cast<const int16_t *>(pcm20ms.constData());
    std::vector<float> floatSamples(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        floatSamples[i] = static_cast<float>(pcmData[i]) / 32768.0f;
    }

    // Stage 1: Melspectrogram
    pairion::core::OnnxTensor melInput{"input", floatSamples, {1, sampleCount}};
    auto melOutputs = m_melSession->run({melInput}, {"output"});

    // Stage 2: Embedding
    pairion::core::OnnxTensor embInput{"input", melOutputs[0].data, melOutputs[0].shape};
    auto embOutputs = m_embeddingSession->run({embInput}, {"output"});

    // Stage 3: Classifier
    pairion::core::OnnxTensor clsInput{"input", embOutputs[0].data, embOutputs[0].shape};
    auto clsOutputs = m_classifierSession->run({clsInput}, {"output"});

    float score = clsOutputs[0].data.empty() ? 0.0f : clsOutputs[0].data[0];

    if (score < m_threshold) {
        return;
    }

    // Check suppression window
    if (m_suppressionActive && m_suppressionTimer.elapsed() < kSuppressionMs) {
        qCDebug(lcWake) << "Wake suppressed, elapsed:" << m_suppressionTimer.elapsed() << "ms";
        return;
    }

    // Fire wake detection
    m_suppressionActive = true;
    m_suppressionTimer.start();
    qCInfo(lcWake) << "Wake word detected, score:" << score;
    emit wakeWordDetected(score, m_preRollBuffer);
}

} // namespace pairion::wake
