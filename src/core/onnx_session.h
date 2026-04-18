#pragma once

/**
 * @file onnx_session.h
 * @brief Abstract ONNX inference session and real OnnxRuntime implementation.
 *
 * Provides a testability seam: wake word and VAD modules receive an
 * OnnxInferenceSession pointer via constructor injection. Production code
 * uses OrtInferenceSession (wrapping Ort::Session); tests use a mock that
 * returns canned outputs.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace pairion::core {

/**
 * @brief A named tensor with flat float data and shape metadata.
 */
struct OnnxTensor {
    std::string name;
    std::vector<float> data;
    std::vector<int64_t> shape;
};

/**
 * @brief Output tensor from ONNX inference.
 */
struct OnnxOutput {
    std::vector<float> data;
    std::vector<int64_t> shape;
};

/**
 * @brief Abstract interface for ONNX model inference.
 *
 * Subclassed by OrtInferenceSession (production) and MockOnnxSession (tests).
 */
class OnnxInferenceSession {
  public:
    virtual ~OnnxInferenceSession() = default;

    /**
     * @brief Run inference with the given input tensors.
     * @param inputs Named input tensors with float data and shapes.
     * @param outputNames Names of the outputs to request.
     * @return Vector of output tensors in the order of outputNames.
     */
    virtual std::vector<OnnxOutput> run(const std::vector<OnnxTensor> &inputs,
                                        const std::vector<std::string> &outputNames) = 0;
};

/**
 * @brief Production ONNX inference session wrapping onnxruntime.
 *
 * Loads a model file on construction. Inference is synchronous and should
 * be called from a worker thread to avoid blocking the UI.
 */
class OrtInferenceSession : public OnnxInferenceSession {
  public:
    /**
     * @brief Load an ONNX model from disk.
     * @param modelPath Filesystem path to the .onnx model file.
     * @throws std::runtime_error if the model cannot be loaded.
     */
    explicit OrtInferenceSession(const std::string &modelPath);
    ~OrtInferenceSession() override;

    std::vector<OnnxOutput> run(const std::vector<OnnxTensor> &inputs,
                                const std::vector<std::string> &outputNames) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace pairion::core
