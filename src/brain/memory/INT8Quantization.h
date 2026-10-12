#pragma once
// INT8Quantization — INT8 quantization for neural network weights and activations
// 4x memory reduction, 2-4x speedup on compatible hardware

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>
#include <string>
#include <memory>

namespace yuki {
namespace memory {

struct QuantizationConfig {
    bool symmetric = true;           // Symmetric quantization ([-127, 127])
    bool per_channel = false;        // Per-channel vs per-tensor
    int8_t quant_min = -127;         // Minimum quantized value
    int8_t quant_max = 127;          // Maximum quantized value
    float eps = 1e-8f;               // Epsilon for numerical stability
};

struct QuantizedTensor {
    std::vector<int8_t> data;        // INT8 quantized data
    std::vector<float> scales;       // Scale per channel (or single scale)
    std::vector<int32_t> zero_points; // Zero point per channel (or single)
    std::vector<size_t> shape;       // Original tensor shape
    size_t num_channels = 1;         // Number of quantization channels
    
    size_t memoryBytes() const {
        return data.size() + scales.size() * sizeof(float) + zero_points.size() * sizeof(int32_t);
    }
    
    size_t originalBytes() const {
        size_t total = 1;
        for (size_t s : shape) total *= s;
        return total * sizeof(float);
    }
    
    float compressionRatio() const {
        return static_cast<float>(originalBytes()) / memoryBytes();
    }
};

class INT8Quantizer {
public:
    explicit INT8Quantizer(const QuantizationConfig& cfg = QuantizationConfig{});
    ~INT8Quantizer() = default;

    void setConfig(const QuantizationConfig& cfg) { config_ = cfg; }
    const QuantizationConfig& getConfig() const { return config_; }

    // Quantize float tensor to INT8
    QuantizedTensor quantize(const std::vector<float>& input, const std::vector<size_t>& shape);
    QuantizedTensor quantizePerChannel(const std::vector<float>& input, 
                                        const std::vector<size_t>& shape,
                                        int channel_axis);

    // Dequantize INT8 back to float
    std::vector<float> dequantize(const QuantizedTensor& qt) const;

    // Quantize matrix (2D) for linear layers
    QuantizedTensor quantizeWeightMatrix(const std::vector<float>& weight,
                                          size_t out_features, size_t in_features,
                                          bool per_output_channel = true);

    // Quantize bias with accumulated scale
    QuantizedTensor quantizeBias(const std::vector<float>& bias,
                                  const std::vector<float>& weight_scales,
                                  const std::vector<float>& input_scales);

    // Fused quantized linear: y = x @ W^T + b (all INT8 with INT32 accumulation)
    std::vector<int32_t> quantizedLinear(const std::vector<int8_t>& input,
                                          const QuantizedTensor& weight,
                                          const QuantizedTensor& bias,
                                          size_t batch_size, size_t in_features, size_t out_features) const;

    // Calibration: collect statistics for quantization parameters
    struct CalibrationData {
        float min_val = (std::numeric_limits<float>::max)();
        float max_val = (std::numeric_limits<float>::lowest)();
        std::vector<float> channel_mins;
        std::vector<float> channel_maxs;
        int channel_axis = -1;
        size_t num_samples = 0;
    };
    
    void calibrate(const std::vector<float>& data, CalibrationData& cal, int channel_axis = -1);
    void finalizeCalibration(CalibrationData& cal, std::vector<float>& scales, std::vector<int32_t>& zero_points);

    // Serialization
    std::string serialize(const QuantizedTensor& qt) const;
    QuantizedTensor deserialize(const std::string& data) const;

private:
    QuantizationConfig config_;

    // Compute scale and zero point from min/max
    void computeScaleZeroPoint(float min_val, float max_val, float& scale, int32_t& zero_point) const;
    void computeScaleZeroPointPerChannel(const std::vector<float>& mins, const std::vector<float>& maxs,
                                          std::vector<float>& scales, std::vector<int32_t>& zero_points) const;
};

} // namespace memory
} // namespace yuki