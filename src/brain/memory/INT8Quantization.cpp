#include "brain/memory/INT8Quantization.h"
#include <numeric>
#include <sstream>
#include <cassert>

namespace yuki {
namespace memory {

INT8Quantizer::INT8Quantizer(const QuantizationConfig& cfg) : config_(cfg) {}

void INT8Quantizer::computeScaleZeroPoint(float min_val, float max_val, float& scale, int32_t& zero_point) const {
    if (config_.symmetric) {
        // Symmetric: range is [-max_abs, max_abs]
        float max_abs = std::max(std::abs(min_val), std::abs(max_val));
        max_abs = std::max(max_abs, config_.eps);
        scale = max_abs / static_cast<float>(config_.quant_max);
        zero_point = 0;
    } else {
        // Asymmetric: range is [min_val, max_val]
        float range = max_val - min_val;
        range = std::max(range, config_.eps);
        scale = range / static_cast<float>(config_.quant_max - config_.quant_min);
        zero_point = static_cast<int32_t>(std::round(-min_val / scale)) + config_.quant_min;
        zero_point = std::clamp(zero_point, static_cast<int32_t>(config_.quant_min), 
                                static_cast<int32_t>(config_.quant_max));
    }
}

void INT8Quantizer::computeScaleZeroPointPerChannel(const std::vector<float>& mins, 
                                                     const std::vector<float>& maxs,
                                                     std::vector<float>& scales,
                                                     std::vector<int32_t>& zero_points) const {
    size_t n = mins.size();
    scales.resize(n);
    zero_points.resize(n);
    
    for (size_t i = 0; i < n; ++i) {
        computeScaleZeroPoint(mins[i], maxs[i], scales[i], zero_points[i]);
    }
}

QuantizedTensor INT8Quantizer::quantize(const std::vector<float>& input, const std::vector<size_t>& shape) {
    QuantizedTensor qt;
    qt.shape = shape;
    
    float min_val = *std::min_element(input.begin(), input.end());
    float max_val = *std::max_element(input.begin(), input.end());
    
    float scale;
    int32_t zero_point;
    computeScaleZeroPoint(min_val, max_val, scale, zero_point);
    
    qt.scales = {scale};
    qt.zero_points = {zero_point};
    qt.num_channels = 1;
    
    qt.data.resize(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        int32_t q = static_cast<int32_t>(std::round(input[i] / scale)) + zero_point;
        q = std::clamp(q, static_cast<int32_t>(config_.quant_min), static_cast<int32_t>(config_.quant_max));
        qt.data[i] = static_cast<int8_t>(q);
    }
    
    return qt;
}

QuantizedTensor INT8Quantizer::quantizePerChannel(const std::vector<float>& input, 
                                                   const std::vector<size_t>& shape,
                                                   int channel_axis) {
    QuantizedTensor qt;
    qt.shape = shape;
    
    if (channel_axis < 0) channel_axis += shape.size();
    assert(channel_axis >= 0 && channel_axis < static_cast<int>(shape.size()));
    
    size_t num_channels = shape[channel_axis];
    size_t channel_size = 1;
    for (size_t i = channel_axis + 1; i < shape.size(); ++i) {
        channel_size *= shape[i];
    }
    size_t outer_size = 1;
    for (size_t i = 0; i < channel_axis; ++i) {
        outer_size *= shape[i];
    }
    
    qt.num_channels = num_channels;
    qt.scales.resize(num_channels);
    qt.zero_points.resize(num_channels);
    qt.data.resize(input.size());
    
    std::vector<float> channel_mins(num_channels, std::numeric_limits<float>::max());
    std::vector<float> channel_maxs(num_channels, std::numeric_limits<float>::lowest());
    
    // Find min/max per channel
    for (size_t outer = 0; outer < outer_size; ++outer) {
        for (size_t c = 0; c < num_channels; ++c) {
            for (size_t inner = 0; inner < channel_size; ++inner) {
                size_t idx = outer * num_channels * channel_size + c * channel_size + inner;
                float val = input[idx];
                channel_mins[c] = std::min(channel_mins[c], val);
                channel_maxs[c] = std::max(channel_maxs[c], val);
            }
        }
    }
    
    // Compute scales and zero points per channel
    computeScaleZeroPointPerChannel(channel_mins, channel_maxs, qt.scales, qt.zero_points);
    
    // Quantize
    for (size_t outer = 0; outer < outer_size; ++outer) {
        for (size_t c = 0; c < num_channels; ++c) {
            float scale = qt.scales[c];
            int32_t zp = qt.zero_points[c];
            for (size_t inner = 0; inner < channel_size; ++inner) {
                size_t idx = outer * num_channels * channel_size + c * channel_size + inner;
                float val = input[idx];
                int32_t q = static_cast<int32_t>(std::round(val / scale)) + zp;
                q = std::clamp(q, static_cast<int32_t>(config_.quant_min), static_cast<int32_t>(config_.quant_max));
                qt.data[idx] = static_cast<int8_t>(q);
            }
        }
    }
    
    return qt;
}

std::vector<float> INT8Quantizer::dequantize(const QuantizedTensor& qt) const {
    std::vector<float> output(qt.data.size());
    
    if (qt.num_channels == 1) {
        float scale = qt.scales[0];
        int32_t zp = qt.zero_points[0];
        for (size_t i = 0; i < qt.data.size(); ++i) {
            output[i] = (static_cast<float>(qt.data[i]) - zp) * scale;
        }
    } else {
        // Per-channel dequantization
        size_t num_channels = qt.num_channels;
        size_t total = qt.data.size();
        size_t channel_size = total / num_channels;
        
        for (size_t c = 0; c < num_channels; ++c) {
            float scale = qt.scales[c];
            int32_t zp = qt.zero_points[c];
            for (size_t i = 0; i < channel_size; ++i) {
                size_t idx = c * channel_size + i;
                output[idx] = (static_cast<float>(qt.data[idx]) - zp) * scale;
            }
        }
    }
    
    return output;
}

QuantizedTensor INT8Quantizer::quantizeWeightMatrix(const std::vector<float>& weight,
                                                     size_t out_features, size_t in_features,
                                                     bool per_output_channel) {
    std::vector<size_t> shape = {out_features, in_features};
    if (per_output_channel) {
        return quantizePerChannel(weight, shape, 0);  // Channel axis 0 = output features
    } else {
        return quantize(weight, shape);
    }
}

QuantizedTensor INT8Quantizer::quantizeBias(const std::vector<float>& bias,
                                             const std::vector<float>& weight_scales,
                                             const std::vector<float>& input_scales) {
    QuantizedTensor qt;
    qt.shape = {bias.size()};
    qt.num_channels = bias.size();
    
    // Bias scale = weight_scale * input_scale (for quantized matmul)
    qt.scales.resize(bias.size());
    for (size_t i = 0; i < bias.size(); ++i) {
        float w_scale = weight_scales[i % weight_scales.size()];
        float x_scale = input_scales[0];  // Assume single input scale
        qt.scales[i] = w_scale * x_scale;
    }
    qt.zero_points.assign(bias.size(), 0);
    
    qt.data.resize(bias.size());
    for (size_t i = 0; i < bias.size(); ++i) {
        float scale = qt.scales[i];
        if (scale > 0) {
            int32_t q = static_cast<int32_t>(std::round(bias[i] / scale));
            q = std::clamp(q, static_cast<int32_t>(config_.quant_min), static_cast<int32_t>(config_.quant_max));
            qt.data[i] = static_cast<int8_t>(q);
        } else {
            qt.data[i] = 0;
        }
    }
    
    return qt;
}

std::vector<int32_t> INT8Quantizer::quantizedLinear(const std::vector<int8_t>& input,
                                                     const QuantizedTensor& weight,
                                                     const QuantizedTensor& bias,
                                                     size_t batch_size, size_t in_features, size_t out_features) const {
    std::vector<int32_t> output(batch_size * out_features);
    
    // weight shape: [out_features, in_features]
    // input shape: [batch_size, in_features]
    // output: [batch_size, out_features]
    
    for (size_t b = 0; b < batch_size; ++b) {
        for (size_t o = 0; o < out_features; ++o) {
            int32_t acc = 0;
            float w_scale = weight.scales[weight.num_channels > 1 ? o : 0];
            int32_t w_zp = weight.zero_points[weight.num_channels > 1 ? o : 0];
            float x_scale = 1.0f;  // Would be passed in real implementation
            
            for (size_t i = 0; i < in_features; ++i) {
                int8_t x = input[b * in_features + i];
                int8_t w = weight.data[o * in_features + i];
                
                // Accumulate in INT32
                acc += static_cast<int32_t>(x) * static_cast<int32_t>(w);
            }
            
            // Add bias (already scaled)
            if (bias.data.size() == out_features) {
                acc += static_cast<int32_t>(bias.data[o]);
            }
            
            // Apply output scale (would be done in dequantization)
            output[b * out_features + o] = acc;
        }
    }
    
    return output;
}

void INT8Quantizer::calibrate(const std::vector<float>& data, CalibrationData& cal, int channel_axis) {
    cal.num_samples += data.size();
    float min_val = *std::min_element(data.begin(), data.end());
    float max_val = *std::max_element(data.begin(), data.end());
    cal.min_val = std::min(cal.min_val, min_val);
    cal.max_val = std::max(cal.max_val, max_val);
    cal.channel_axis = channel_axis;
    
    if (channel_axis >= 0) {
        // Per-channel calibration
        // This is simplified - real implementation would compute per-channel min/max
        if (cal.channel_mins.empty()) {
            cal.channel_mins.resize(1, min_val);
            cal.channel_maxs.resize(1, max_val);
        }
    }
}

void INT8Quantizer::finalizeCalibration(CalibrationData& cal, 
                                         std::vector<float>& scales, 
                                         std::vector<int32_t>& zero_points) {
    if (!cal.channel_mins.empty()) {
        computeScaleZeroPointPerChannel(cal.channel_mins, cal.channel_maxs, scales, zero_points);
    } else {
        float scale;
        int32_t zp;
        computeScaleZeroPoint(cal.min_val, cal.max_val, scale, zp);
        scales = {scale};
        zero_points = {zp};
    }
}

std::string INT8Quantizer::serialize(const QuantizedTensor& qt) const {
    std::ostringstream oss;
    oss << "{\"shape\":[";
    for (size_t i = 0; i < qt.shape.size(); ++i) {
        oss << qt.shape[i];
        if (i + 1 < qt.shape.size()) oss << ",";
    }
    oss << "],\"scales\":[";
    for (size_t i = 0; i < qt.scales.size(); ++i) {
        oss << qt.scales[i];
        if (i + 1 < qt.scales.size()) oss << ",";
    }
    oss << "],\"zero_points\":[";
    for (size_t i = 0; i < qt.zero_points.size(); ++i) {
        oss << qt.zero_points[i];
        if (i + 1 < qt.zero_points.size()) oss << ",";
    }
    oss << "],\"data\":[";
    for (size_t i = 0; i < qt.data.size(); ++i) {
        oss << static_cast<int>(qt.data[i]);
        if (i + 1 < qt.data.size()) oss << ",";
    }
    oss << "],\"channels\":" << qt.num_channels << "}";
    return oss.str();
}

QuantizedTensor INT8Quantizer::deserialize(const std::string& data) const {
    QuantizedTensor qt;
    // Simplified - would use JSON parser in production
    return qt;
}

} // namespace memory
} // namespace yuki