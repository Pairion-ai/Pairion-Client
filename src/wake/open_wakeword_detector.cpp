/**
 * @file open_wakeword_detector.cpp
 * @brief Implementation of the openWakeWord three-model detector.
 *
 * Data flow matching the openWakeWord Python library:
 *   1. Accumulate raw PCM into a sliding buffer
 *   2. Every 1280 samples (80ms), run melspectrogram on the last N+480 samples
 *      (480 = 160*3 samples of context for mel window overlap)
 *   3. Append mel rows to a rolling mel buffer, take last 76 for embedding
 *   4. Append embedding features to a rolling feature buffer
 *   5. Take last 16 features for classifier → score
 */

#include "open_wakeword_detector.h"

#include <algorithm>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcWake, "pairion.wake")

namespace pairion::wake {

OpenWakewordDetector::OpenWakewordDetector(pairion::core::OnnxInferenceSession *melSession,
                                           pairion::core::OnnxInferenceSession *embeddingSession,
                                           pairion::core::OnnxInferenceSession *classifierSession,
                                           double threshold, QObject *parent)
    : QObject(parent), m_melSession(melSession), m_embeddingSession(embeddingSession),
      m_classifierSession(classifierSession), m_threshold(threshold) {
    // Pre-fill the raw audio buffer with silence so the mel model has enough
    // context from the very first real audio chunk. The mel model needs at least
    // 1280 + 480 = 1760 samples. We pre-fill with enough for the full warmup.
    m_rawAudioBuffer.resize(kMelContextSamples, 0.0f);
}

void OpenWakewordDetector::processPcmFrame(const QByteArray &pcm20ms) {
    // Maintain pre-roll buffer (rolling window of ~200 ms raw PCM)
    m_preRollBuffer.append(pcm20ms);
    if (m_preRollBuffer.size() > kPreRollBytes) {
        m_preRollBuffer = m_preRollBuffer.right(kPreRollBytes);
    }

    // Convert int16 to float32 and append to raw audio buffer
    int sampleCount = pcm20ms.size() / 2;
    const auto *pcmData = reinterpret_cast<const int16_t *>(pcm20ms.constData());
    for (int i = 0; i < sampleCount; ++i) {
        m_rawAudioBuffer.push_back(static_cast<float>(pcmData[i]) / 32768.0f);
    }
    m_accumulatedSamples += sampleCount;

    // Process when we have accumulated 1280 samples (80ms)
    if (m_accumulatedSamples >= kMelChunkSamples) {
        processAccumulatedAudio();
        m_accumulatedSamples = 0;
    }

    // Trim raw audio buffer to prevent unbounded growth
    if (static_cast<int>(m_rawAudioBuffer.size()) > kMaxRawBufferSamples) {
        m_rawAudioBuffer.erase(
            m_rawAudioBuffer.begin(),
            m_rawAudioBuffer.begin() +
                (static_cast<int>(m_rawAudioBuffer.size()) - kMaxRawBufferSamples));
    }
}

void OpenWakewordDetector::processAccumulatedAudio() {
    // Feed the mel model with the last chunk + 480 samples of context,
    // matching the Python library's _streaming_melspectrogram behavior.
    int totalSamples = static_cast<int>(m_rawAudioBuffer.size());
    int feedSamples = std::min(totalSamples, m_accumulatedSamples + kMelContextSamples);
    int startIdx = totalSamples - feedSamples;

    std::vector<float> melInput(m_rawAudioBuffer.begin() + startIdx, m_rawAudioBuffer.end());

    // Stage 1: Melspectrogram
    pairion::core::OnnxTensor melTensor{
        "input", melInput, {}, {1, static_cast<int64_t>(melInput.size())}};
    auto melOutputs = m_melSession->run({melTensor}, {"output"});

    if (melOutputs.empty() || melOutputs[0].data.empty()) {
        return;
    }

    // Extract mel rows (output is [1, 1, N, 32]), append to rolling buffer
    const auto &melData = melOutputs[0].data;
    int numMelRows = static_cast<int>(melData.size()) / kMelFrameWidth;
    // Only take the last rows corresponding to the new chunk
    // (Python takes all rows produced and appends them)
    for (int f = 0; f < numMelRows; ++f) {
        std::vector<float> row(melData.begin() + f * kMelFrameWidth,
                               melData.begin() + (f + 1) * kMelFrameWidth);
        m_melBuffer.push_back(std::move(row));
    }

    // Trim mel buffer to max length
    while (static_cast<int>(m_melBuffer.size()) > kMelBufferMaxLen) {
        m_melBuffer.pop_front();
    }

    // Stage 2: Embedding — need at least 76 mel rows
    if (static_cast<int>(m_melBuffer.size()) < kMelFramesNeeded) {
        return;
    }

    // Take last 76 mel rows, reshape to [1, 76, 32, 1]
    int startRow = static_cast<int>(m_melBuffer.size()) - kMelFramesNeeded;
    std::vector<float> embInput(kMelFramesNeeded * kMelFrameWidth);
    for (int i = 0; i < kMelFramesNeeded; ++i) {
        const auto &row = m_melBuffer[startRow + i];
        std::copy(row.begin(), row.end(), embInput.begin() + i * kMelFrameWidth);
    }

    pairion::core::OnnxTensor embTensor{
        "input_1", embInput, {}, {1, kMelFramesNeeded, kMelFrameWidth, 1}};
    auto embOutputs = m_embeddingSession->run({embTensor}, {"conv2d_19"});

    if (embOutputs.empty() || embOutputs[0].data.empty()) {
        return;
    }

    // Append embedding feature to rolling buffer
    m_embFeatures.push_back(embOutputs[0].data);
    while (static_cast<int>(m_embFeatures.size()) > kFeatureBufferMaxLen) {
        m_embFeatures.pop_front();
    }

    // Stage 3: Classifier — need at least 16 embedding features
    if (static_cast<int>(m_embFeatures.size()) < kEmbFeaturesNeeded) {
        return;
    }

    runClassifier();
}

void OpenWakewordDetector::runClassifier() {
    // Take last 16 embedding features, reshape to [1, 16, 96]
    int startFeat = static_cast<int>(m_embFeatures.size()) - kEmbFeaturesNeeded;
    std::vector<float> clsInput(kEmbFeaturesNeeded * kEmbFeatureSize);
    for (int i = 0; i < kEmbFeaturesNeeded; ++i) {
        const auto &feat = m_embFeatures[startFeat + i];
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

    if (score > 0.1) {
        qCInfo(lcWake) << "Classifier score:" << score;
    }

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
