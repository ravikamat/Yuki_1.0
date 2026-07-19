#include "input/encoding/VisualEncoder.h"
#include <cmath>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace yuki::perception {

static inline float randNormal_v(std::mt19937& rng, float mean, float stddev) {
    std::normal_distribution<float> dist(mean, stddev);
    return dist(rng);
}

VisualEncoder::VisualEncoder(int project_dim) : project_dim_(project_dim), rng_(42) {
    initProjectionMatrix();
}

void VisualEncoder::initProjectionMatrix() {
    const int source_dim = 32;
    projection_.resize(static_cast<size_t>(source_dim) * static_cast<size_t>(project_dim_));
    float scale = 1.0f / std::sqrt(static_cast<float>(source_dim));
    for (size_t i = 0; i < projection_.size(); ++i) {
        projection_[i] = randNormal_v(rng_, 0.0f, scale);
    }
}

std::vector<float> VisualEncoder::toGrayscale(const ImageBuffer& img) const {
    int n = img.width * img.height;
    std::vector<float> gray(static_cast<size_t>(n), 0.0f);
    if (img.channels == 3) {
        for (int i = 0; i < n; ++i) {
            int idx = i * 3;
            gray[static_cast<size_t>(i)] =
                0.299f  * static_cast<float>(img.data[idx])     +
                0.587f  * static_cast<float>(img.data[idx + 1]) +
                0.114f  * static_cast<float>(img.data[idx + 2]);
        }
    } else if (img.channels == 1) {
        for (int i = 0; i < n; ++i)
            gray[static_cast<size_t>(i)] = static_cast<float>(img.data[i]);
    } else {
        throw std::runtime_error("VisualEncoder: unsupported channel count");
    }
    // Normalize to [0,1]
    float max_val = 1.0f;
    for (float v : gray) if (v > max_val) max_val = v;
    if (max_val > 1.0f) for (auto& v : gray) v /= max_val;
    return gray;
}

void VisualEncoder::gaussianBlur3x3(std::vector<float>& gray, int w, int h) const {
    std::vector<float> tmp(static_cast<size_t>(w * h));
    // Horizontal pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float v     = gray[static_cast<size_t>(y * w + x)];
            float left  = (x > 0)     ? gray[static_cast<size_t>(y * w + (x - 1))] : v;
            float right = (x < w - 1) ? gray[static_cast<size_t>(y * w + (x + 1))] : v;
            tmp[static_cast<size_t>(y * w + x)] = (left + 2.0f * v + right) * 0.25f;
        }
    }
    // Vertical pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float v    = tmp[static_cast<size_t>(y * w + x)];
            float up   = (y > 0)     ? tmp[static_cast<size_t>((y - 1) * w + x)] : v;
            float down = (y < h - 1) ? tmp[static_cast<size_t>((y + 1) * w + x)] : v;
            gray[static_cast<size_t>(y * w + x)] = (up + 2.0f * v + down) * 0.25f;
        }
    }
}

void VisualEncoder::sobelGradients(const std::vector<float>& gray, int w, int h,
                                   std::vector<float>& mag, std::vector<float>& ori) const {
    size_t n = static_cast<size_t>(w * h);
    mag.assign(n, 0.0f);
    ori.assign(n, 0.0f);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float gx =
                -1.0f * gray[static_cast<size_t>((y-1)*w+(x-1))] + 1.0f * gray[static_cast<size_t>((y-1)*w+(x+1))]
                -2.0f * gray[static_cast<size_t>(y*w+(x-1))]     + 2.0f * gray[static_cast<size_t>(y*w+(x+1))]
                -1.0f * gray[static_cast<size_t>((y+1)*w+(x-1))] + 1.0f * gray[static_cast<size_t>((y+1)*w+(x+1))];
            float gy =
                -1.0f * gray[static_cast<size_t>((y-1)*w+(x-1))] - 2.0f * gray[static_cast<size_t>((y-1)*w+x)] - 1.0f * gray[static_cast<size_t>((y-1)*w+(x+1))]
                +1.0f * gray[static_cast<size_t>((y+1)*w+(x-1))] + 2.0f * gray[static_cast<size_t>((y+1)*w+x)] + 1.0f * gray[static_cast<size_t>((y+1)*w+(x+1))];
            size_t idx = static_cast<size_t>(y * w + x);
            mag[idx] = std::sqrt(gx * gx + gy * gy);
            ori[idx] = std::atan2(gy, gx);
        }
    }
}

std::vector<float> VisualEncoder::computeHOG(const std::vector<float>& mag,
                                              const std::vector<float>& ori,
                                              int w, int h) const {
    const int num_cells_x = 2;
    const int num_cells_y = 2;
    const int num_bins    = 8;
    const int total_dims  = num_cells_x * num_cells_y * num_bins; // 32

    std::vector<float> hog(static_cast<size_t>(total_dims), 0.0f);

    int cell_w = std::max(1, w / num_cells_x);
    int cell_h = std::max(1, h / num_cells_y);

    for (int cy = 0; cy < num_cells_y; ++cy) {
        for (int cx = 0; cx < num_cells_x; ++cx) {
            int base_idx = (cy * num_cells_x + cx) * num_bins;
            int y_start  = cy * cell_h;
            int y_end    = std::min((cy + 1) * cell_h, h);
            int x_start  = cx * cell_w;
            int x_end    = std::min((cx + 1) * cell_w, w);

            for (int y = y_start; y < y_end; ++y) {
                for (int x = x_start; x < x_end; ++x) {
                    size_t idx  = static_cast<size_t>(y * w + x);
                    float angle = ori[idx];
                    if (angle < 0.0f) angle += 2.0f * static_cast<float>(M_PI);
                    float bin_size   = (2.0f * static_cast<float>(M_PI)) / static_cast<float>(num_bins);
                    float bin_f      = angle / bin_size;
                    int   bin_low    = static_cast<int>(std::floor(bin_f)) % num_bins;
                    int   bin_high   = (bin_low + 1) % num_bins;
                    float weight_high = bin_f - std::floor(bin_f);
                    float weight_low  = 1.0f - weight_high;
                    hog[static_cast<size_t>(base_idx + bin_low)]  += mag[idx] * weight_low;
                    hog[static_cast<size_t>(base_idx + bin_high)] += mag[idx] * weight_high;
                }
            }
            // Normalize cell
            float cell_sq = 0.0f;
            for (int b = 0; b < num_bins; ++b)
                cell_sq += hog[static_cast<size_t>(base_idx + b)] * hog[static_cast<size_t>(base_idx + b)];
            if (cell_sq > 1e-8f) {
                float inv = 1.0f / std::sqrt(cell_sq);
                for (int b = 0; b < num_bins; ++b) hog[static_cast<size_t>(base_idx + b)] *= inv;
            }
        }
    }
    // Global L2 normalize
    float global_sq = 0.0f;
    for (float v : hog) global_sq += v * v;
    if (global_sq > 1e-8f) {
        float inv = 1.0f / std::sqrt(global_sq);
        for (auto& v : hog) v *= inv;
    }
    return hog;
}

std::vector<float> VisualEncoder::encode(const ImageBuffer& img) const {
    if (!img.data || img.width <= 0 || img.height <= 0)
        return std::vector<float>(32, 0.0f);
    auto gray = toGrayscale(img);
    gaussianBlur3x3(gray, img.width, img.height);
    std::vector<float> mag, ori;
    sobelGradients(gray, img.width, img.height, mag, ori);
    return computeHOG(mag, ori, img.width, img.height);
}

std::vector<float> VisualEncoder::projectTo8(const std::vector<float>& vec32) const {
    if (static_cast<int>(vec32.size()) != 32)
        throw std::runtime_error("projectTo8: dim mismatch (expected 32)");
    std::vector<float> out(static_cast<size_t>(project_dim_), 0.0f);
    for (int j = 0; j < project_dim_; ++j) {
        float sum = 0.0f;
        for (int i = 0; i < 32; ++i)
            sum += vec32[static_cast<size_t>(i)] * projection_[static_cast<size_t>(i * project_dim_ + j)];
        out[static_cast<size_t>(j)] = sum;
    }
    // L2 normalize
    float sq = 0.0f;
    for (float v : out) sq += v * v;
    if (sq > 1e-8f) {
        float inv = 1.0f / std::sqrt(sq);
        for (auto& v : out) v *= inv;
    } else {
        float val = 1.0f / std::sqrt(static_cast<float>(project_dim_));
        for (auto& v : out) v = val;
    }
    return out;
}

} // namespace yuki::perception
