/**
 * @file open_wakeword_detector.cpp
 * @brief Implementation of the openWakeWord three-model detector.
 *
 * Data flow per the openWakeWord architecture:
 *   1. Accumulate 1280 PCM samples (80 ms)
 *   2. Run melspectrogram → output [1,1,5,32] → store 5 mel frames (each 32 floats)
 *   3. When 76 mel frames accumulated, run embedding → output [1,1,1,96]
 *   4. When 16 embedding features accumulated, run classifier → output [1,1] score
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

    // Accumulate PCM until we have 1280 samples (2560 bytes) for mel
    m_pcmAccumulator.append(pcm20ms);

    while (m_pcmAccumulator.size() >= kMelChunkBytes) {
        QByteArray chunk = m_pcmAccumulator.left(kMelChunkBytes);
        m_pcmAccumulator.remove(0, kMelChunkBytes);

        // Convert int16 to float32
        const auto *pcmData = reinterpret_cast<const int16_t *>(chunk.constData());
        std::vector<float> floatSamples(kMelChunkSamples);
        for (int i = 0; i < kMelChunkSamples; ++i) {
            floatSamples[i] = static_cast<float>(pcmData[i]) / 32768.0f;
        }

        processChunk(floatSamples);
    }
}

void OpenWakewordDetector::processChunk(const std::vector<float> &samples1280) {
    // Stage 1: Melspectrogram — input [1, 1280], output [1, 1, 5, 32]
    pairion::core::OnnxTensor melInput{"input", samples1280, {}, {1, kMelChunkSamples}};
    auto melOutputs = m_melSession->run({melInput}, {"output"});

    if (melOutputs.empty() || melOutputs[0].data.empty()) {
        return;
    }

    // Extract mel frames: output is [1, 1, N, 32], take each row of 32 floats
    const auto &melData = melOutputs[0].data;
    int totalMelFloats = static_cast<int>(melData.size());
    int numFrames = totalMelFloats / kMelFrameWidth;
    for (int f = 0; f < numFrames; ++f) {
        std::vector<float> frame(melData.begin() + f * kMelFrameWidth,
                                 melData.begin() + (f + 1) * kMelFrameWidth);
        m_melFrames.push_back(std::move(frame));
    }

    // Trim to keep only the last kMelFramesNeeded frames
    while (static_cast<int>(m_melFrames.size()) > kMelFramesNeeded) {
        m_melFrames.pop_front();
    }

    // Stage 2: Embedding — need exactly 76 mel frames
    if (static_cast<int>(m_melFrames.size()) < kMelFramesNeeded) {
        return;
    }

    // Build embedding input [1, 76, 32, 1]
    std::vector<float> embInput(kMelFramesNeeded * kMelFrameWidth);
    for (int i = 0; i < kMelFramesNeeded; ++i) {
        std::copy(m_melFrames[i].begin(), m_melFrames[i].end(),
                  embInput.begin() + i * kMelFrameWidth);
    }

    pairion::core::OnnxTensor embTensor{
        "input_1", embInput, {}, {1, kMelFramesNeeded, kMelFrameWidth, 1}};
    auto embOutputs = m_embeddingSession->run({embTensor}, {"conv2d_19"});

    if (embOutputs.empty() || embOutputs[0].data.empty()) {
        return;
    }

    // Extract embedding feature [1, 1, 1, 96] → 96 floats
    m_embFeatures.push_back(embOutputs[0].data);

    // Trim to keep only the last kEmbFeaturesNeeded
    while (static_cast<int>(m_embFeatures.size()) > kEmbFeaturesNeeded) {
        m_embFeatures.pop_front();
    }

    // Stage 3: Classifier — need exactly 16 embedding features
    if (static_cast<int>(m_embFeatures.size()) >= kEmbFeaturesNeeded) {
        runClassifier();
    }
}

void OpenWakewordDetector::runClassifier() {
    // Build classifier input [1, 16, 96]
    std::vector<float> clsInput(kEmbFeaturesNeeded * kEmbFeatureSize);
    for (int i = 0; i < kEmbFeaturesNeeded; ++i) {
        const auto &feat = m_embFeatures[i];
        int copySize = std::min(static_cast<int>(feat.size()), kEmbFeatureSize);
        std::copy(feat.begin(), feat.begin() + copySize, clsInput.begin() + i * kEmbFeatureSize);
    }

    pairion::core::OnnxTensor clsTensor{
        "x.1", clsInput, {}, {1, kEmbFeaturesNeeded, kEmbFeatureSize}};
    auto clsOutputs = m_classifierSession->run({clsTensor}, {"53"});

    if (clsOutputs.empty() || clsOutputs[0].data.empty()) {
        return;
    }

    float score = clsOutputs[0].data[0];

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
