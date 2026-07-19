#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <random>
#include <algorithm>
#include <stdexcept>

namespace yuki::perception {

struct ImageBuffer {
    const uint8_t* data;
    int width;
    int height;
    int channels;
};

class VisualEncoder {
public:
    explicit VisualEncoder(int project_dim = 8);
    std::vector<float> encode(const ImageBuffer& img) const;
    std::vector<float> projectTo8(const std::vector<float>& vec32) const;

private:
    int project_dim_;
    std::vector<float> projection_;
    mutable std::mt19937 rng_;

    void initProjectionMatrix();
    std::vector<float> toGrayscale(const ImageBuffer& img) const;
    void gaussianBlur3x3(std::vector<float>& gray, int w, int h) const;
    void sobelGradients(const std::vector<float>& gray, int w, int h,
                        std::vector<float>& mag, std::vector<float>& ori) const;
    std::vector<float> computeHOG(const std::vector<float>& mag,
                                  const std::vector<float>& ori,
                                  int w, int h) const;
};

} // namespace yuki::perception
