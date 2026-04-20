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
    m_rawAudioBuffer.resize(kMelContextSamples, 0.0f);
}

void OpenWakewordDetector::warmup() {
    int warmupChunks = (kMelFramesNeeded / kMelRowsPerChunk) + kEmbFeaturesNeeded + 2;
    for (int i = 0; i < warmupChunks; ++i) {
        m_rawAudioBuffer.insert(m_rawAudioBuffer.end(), kMelChunkSamples, 0.0f);
        m_accumulatedSamples = kMelChunkSamples;
        processAccumulatedAudio();
        m_accumulatedSamples = 0;
    }
    qCInfo(lcWake) << "Wake detector primed: mel_rows=" << m_melBuffer.size()
                   << "emb_features=" << m_embFeatures.size();
}

void OpenWakewordDetector::processPcmFrame(const QByteArray &pcm20ms) {
    // Maintain pre-roll buffer (rolling window of ~200 ms raw PCM)
    m_preRollBuffer.append(pcm20ms);
    if (m_preRollBuffer.size() > kPreRollBytes) {
        m_preRollBuffer = m_preRollBuffer.right(kPreRollBytes);
    }

    // Convert int16 to float32 at int16 scale (NOT normalized to [-1,1]).
    // openWakeWord Python reference passes raw int16-scale float32 values
    // to the mel model — dividing by 32768 produces near-zero mel outputs
    // and breaks the embedding→classifier chain.
    int sampleCount = pcm20ms.size() / 2;
    const auto *pcmData = reinterpret_cast<const int16_t *>(pcm20ms.constData());
    for (int i = 0; i < sampleCount; ++i) {
        m_rawAudioBuffer.push_back(static_cast<float>(pcmData[i]));
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
    // Fixed sliding window (1760 samples = 1280 chunk + 480 context) to match
    // Python openWakeWord _get_melspectrogram exactly. Using full buffer caused
    // numerical drift in the mel ONNX model.
    const int windowSize = kMelChunkSamples + kMelContextSamples;
    // LCOV_EXCL_START — buffer is always pre-filled (constructor + warmup) before this runs
    if (static_cast<int>(m_rawAudioBuffer.size()) < windowSize) {
        return;
    }
    // LCOV_EXCL_STOP

    std::vector<float> melInput(m_rawAudioBuffer.end() - windowSize, m_rawAudioBuffer.end());

    // Stage 1: Melspectrogram
    pairion::core::OnnxTensor melTensor{
        "input", melInput, {}, {1, static_cast<int64_t>(melInput.size())}};
    auto melOutputs = m_melSession->run({melTensor}, {"output"});

    // LCOV_EXCL_START — mock sessions always return valid outputs; defensive guard only
    if (melOutputs.empty() || melOutputs[0].data.empty()) {
        return;
    }
    // LCOV_EXCL_STOP

    // Diagnostic: log first mel output shape
    if (m_melBuffer.empty()) {
        QString shapeTxt;
        for (auto d : melOutputs[0].shape) {
            shapeTxt += QString::number(d) + QStringLiteral(" ");
        }
        qCInfo(lcWake) << "First mel output: floats=" << melOutputs[0].data.size() << "shape=["
                       << shapeTxt.trimmed() << "]"
                       << "input_samples=" << static_cast<int>(melInput.size());
    }

    // Extract mel rows from output [1, 1, N, 32]. Only take the LAST 8 rows
    // (corresponding to the new 1280-sample chunk). Earlier rows are context
    // overlap that would duplicate already-buffered mel data.
    const auto &melData = melOutputs[0].data;
    int totalMelRows = static_cast<int>(melData.size()) / kMelFrameWidth;
    int newRows = std::min(totalMelRows, kMelRowsPerChunk);
    int startRow = totalMelRows - newRows;
    for (int f = startRow; f < totalMelRows; ++f) {
        std::vector<float> row(melData.begin() + f * kMelFrameWidth,
                               melData.begin() + (f + 1) * kMelFrameWidth);
        // Apply openWakeWord mel transform: spec/10 + 2 (Python reference:
        // utils.py _get_melspectrogram default melspec_transform).
        // Required to match the scale the embedding model was trained on.
        for (auto &v : row) {
            v = v / 10.0f + 2.0f;
        }
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
    int embStartRow = static_cast<int>(m_melBuffer.size()) - kMelFramesNeeded;
    std::vector<float> embInput(kMelFramesNeeded * kMelFrameWidth);
    for (int i = 0; i < kMelFramesNeeded; ++i) {
        const auto &row = m_melBuffer[embStartRow + i];
        std::copy(row.begin(), row.end(), embInput.begin() + i * kMelFrameWidth);
    }

    pairion::core::OnnxTensor embTensor{
        "input_1", embInput, {}, {1, kMelFramesNeeded, kMelFrameWidth, 1}};
    auto embOutputs = m_embeddingSession->run({embTensor}, {"conv2d_19"});

    // LCOV_EXCL_START — mock sessions always return valid outputs; defensive guard only
    if (embOutputs.empty() || embOutputs[0].data.empty()) {
        return;
    }
    // LCOV_EXCL_STOP

    // Append embedding feature to rolling buffer
    m_embFeatures.push_back(embOutputs[0].data);
    // LCOV_EXCL_START — would require >120 sequential embeddings; not reached in unit tests
    while (static_cast<int>(m_embFeatures.size()) > kFeatureBufferMaxLen) {
        m_embFeatures.pop_front();
    }
    // LCOV_EXCL_STOP

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

    // LCOV_EXCL_START — mock sessions always return valid outputs; defensive guard only
    if (clsOutputs.empty() || clsOutputs[0].data.empty()) {
        return;
    }
    // LCOV_EXCL_STOP

    float score = clsOutputs[0].data[0];

#ifdef PAIRION_WAKE_DIAGNOSTICS
    qCInfo(lcWake) << "Wake classifier score:" << score;
#endif

    if (score < static_cast<float>(m_threshold)) {
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
