#pragma once

/**
 * @file mock_onnx_session.h
 * @brief Mock ONNX inference session returning canned outputs for testing.
 *
 * Used by tst_wake_detector and tst_silero_vad to test logic without
 * requiring real ONNX model files.
 */

#include "../src/core/onnx_session.h"

#include <queue>

namespace pairion::test {

/**
 * @brief Mock OnnxInferenceSession that returns pre-programmed outputs.
 *
 * Callers push expected outputs via enqueueOutput(). Each call to run()
 * dequeues and returns the next set of outputs. If the queue is empty,
 * returns a single output with a zero-filled tensor.
 */
class MockOnnxSession : public pairion::core::OnnxInferenceSession {
  public:
    /**
     * @brief Enqueue a set of outputs for the next run() call.
     * @param outputs The outputs to return.
     */
    void enqueueOutput(const std::vector<pairion::core::OnnxOutput> &outputs) {
        m_outputs.push(outputs);
    }

    /**
     * @brief Number of run() calls made so far.
     */
    int runCount() const { return m_runCount; }

    /**
     * @brief The inputs from the most recent run() call.
     */
    const std::vector<pairion::core::OnnxTensor> &lastInputs() const { return m_lastInputs; }

    std::vector<pairion::core::OnnxOutput>
    run(const std::vector<pairion::core::OnnxTensor> &inputs,
        const std::vector<std::string> &outputNames) override {
        m_runCount++;
        m_lastInputs = inputs;

        if (!m_outputs.empty()) {
            auto result = m_outputs.front();
            m_outputs.pop();
            return result;
        }

        // Default: return zero-filled outputs matching the requested count
        std::vector<pairion::core::OnnxOutput> defaults;
        for (size_t i = 0; i < outputNames.size(); ++i) {
            defaults.push_back(pairion::core::OnnxOutput{{0.0f}, {1}});
        }
        return defaults;
    }

  private:
    std::queue<std::vector<pairion::core::OnnxOutput>> m_outputs;
    std::vector<pairion::core::OnnxTensor> m_lastInputs;
    int m_runCount = 0;
};

} // namespace pairion::test
