/**
 * @file onnx_session.cpp
 * @brief Implementation of OrtInferenceSession wrapping the onnxruntime C++ API.
 */

#include "onnx_session.h"

#include <onnxruntime_cxx_api.h>
#include <QLoggingCategory>
#include <stdexcept>

Q_LOGGING_CATEGORY(lcOnnx, "pairion.onnx")

namespace pairion::core {

struct OrtInferenceSession::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "pairion"};
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault);
};

OrtInferenceSession::OrtInferenceSession(const std::string &modelPath)
    : m_impl(std::make_unique<Impl>()) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    opts.SetInterOpNumThreads(1);
    opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
    m_impl->session = std::make_unique<Ort::Session>(m_impl->env, modelPath.c_str(), opts);
    qCInfo(lcOnnx) << "Loaded ONNX model:" << QString::fromStdString(modelPath);
}

OrtInferenceSession::~OrtInferenceSession() = default;

std::vector<OnnxOutput> OrtInferenceSession::run(const std::vector<OnnxTensor> &inputs,
                                                 const std::vector<std::string> &outputNames) {
    // Build input name pointers
    std::vector<const char *> inputNamePtrs;
    inputNamePtrs.reserve(inputs.size());
    for (const auto &inp : inputs) {
        inputNamePtrs.push_back(inp.name.c_str());
    }

    // Build input tensors (data borrowed, not copied)
    std::vector<Ort::Value> inputValues;
    inputValues.reserve(inputs.size());
    for (const auto &inp : inputs) {
        if (inp.isInt64()) {
            auto *dataPtr = const_cast<int64_t *>(inp.int64Data.data());
            inputValues.push_back(
                Ort::Value::CreateTensor<int64_t>(m_impl->memInfo, dataPtr, inp.int64Data.size(),
                                                  inp.shape.data(), inp.shape.size()));
        } else {
            auto *dataPtr = const_cast<float *>(inp.data.data());
            inputValues.push_back(Ort::Value::CreateTensor<float>(
                m_impl->memInfo, dataPtr, inp.data.size(), inp.shape.data(), inp.shape.size()));
        }
    }

    // Build output name pointers
    std::vector<const char *> outputNamePtrs;
    outputNamePtrs.reserve(outputNames.size());
    for (const auto &name : outputNames) {
        outputNamePtrs.push_back(name.c_str());
    }

    // Run inference
    Ort::RunOptions runOpts;
    auto outputs =
        m_impl->session->Run(runOpts, inputNamePtrs.data(), inputValues.data(), inputValues.size(),
                             outputNamePtrs.data(), outputNamePtrs.size());

    // Extract results
    std::vector<OnnxOutput> result;
    result.reserve(outputs.size());
    for (auto &out : outputs) {
        auto info = out.GetTensorTypeAndShapeInfo();
        auto shape = info.GetShape();
        auto count = info.GetElementCount();
        const float *data = out.GetTensorData<float>();
        result.push_back(OnnxOutput{std::vector<float>(data, data + count), shape});
    }

    return result;
}

} // namespace pairion::core
