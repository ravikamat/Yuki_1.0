#include <iostream>
#include <cmath>
#include <vector>
#include <cstdint>
#include "input/encoding/VisualEncoder.h"

using namespace yuki::perception;

static bool approxEqual(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i]; na += a[i] * a[i]; nb += b[i] * b[i];
    }
    return dot / (std::sqrt(na) * std::sqrt(nb) + 1e-8f);
}

int main() {
    int pass = 0, fail = 0;

    // Test 1: Solid red image → 32D finite HOG
    {
        std::vector<uint8_t> img(64 * 64 * 3, 0);
        for (int i = 0; i < 64 * 64; ++i) { img[static_cast<size_t>(i*3)] = 255; img[static_cast<size_t>(i*3+1)] = 0; img[static_cast<size_t>(i*3+2)] = 0; }
        ImageBuffer buf{img.data(), 64, 64, 3};
        VisualEncoder enc;
        auto feat = enc.encode(buf);
        bool ok = (feat.size() == 32);
        for (float v : feat) if (std::isnan(v) || std::isinf(v)) ok = false;
        if (ok) { std::cout << "[PASS] Red image 32D finite\n"; pass++; }
        else { std::cout << "[FAIL] Red image NaN/Inf or wrong dim\n"; fail++; }
    }

    // Test 2: Horizontal stripes → no NaN
    {
        std::vector<uint8_t> img(64 * 64 * 3, 0);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                uint8_t v = static_cast<uint8_t>((y % 8 < 4) ? 255 : 0);
                img[static_cast<size_t>((y*64+x)*3)]   = v;
                img[static_cast<size_t>((y*64+x)*3+1)] = v;
                img[static_cast<size_t>((y*64+x)*3+2)] = v;
            }
        ImageBuffer buf{img.data(), 64, 64, 3};
        VisualEncoder enc;
        auto feat = enc.encode(buf);
        bool ok = true;
        for (float v : feat) if (std::isnan(v) || std::isinf(v)) ok = false;
        if (ok) { std::cout << "[PASS] Horizontal stripes no NaN\n"; pass++; }
        else { std::cout << "[FAIL] Horizontal stripes NaN\n"; fail++; }
    }

    // Test 3: Vertical stripes → no NaN
    {
        std::vector<uint8_t> img(64 * 64 * 3, 0);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                uint8_t v = static_cast<uint8_t>((x % 8 < 4) ? 255 : 0);
                img[static_cast<size_t>((y*64+x)*3)]   = v;
                img[static_cast<size_t>((y*64+x)*3+1)] = v;
                img[static_cast<size_t>((y*64+x)*3+2)] = v;
            }
        ImageBuffer buf{img.data(), 64, 64, 3};
        VisualEncoder enc;
        auto feat = enc.encode(buf);
        bool ok = true;
        for (float v : feat) if (std::isnan(v) || std::isinf(v)) ok = false;
        if (ok) { std::cout << "[PASS] Vertical stripes no NaN\n"; pass++; }
        else { std::cout << "[FAIL] Vertical stripes NaN\n"; fail++; }
    }

    // Test 4: projectTo8 output has reasonable norm
    {
        VisualEncoder enc;
        std::vector<float> dummy(32, 0.5f);
        auto proj = enc.projectTo8(dummy);
        float sq = 0.0f;
        for (float v : proj) sq += v * v;
        float norm = std::sqrt(sq);
        if (norm >= 0.5f && norm <= 2.0f) {
            std::cout << "[PASS] projectTo8 norm=" << norm << "\n"; pass++;
        } else {
            std::cout << "[FAIL] projectTo8 norm=" << norm << "\n"; fail++;
        }
    }

    // Test 5: Identical images → cosine similarity > 0.99
    {
        std::vector<uint8_t> img(64 * 64 * 3, 128);
        ImageBuffer buf{img.data(), 64, 64, 3};
        VisualEncoder enc;
        auto a = enc.projectTo8(enc.encode(buf));
        auto b = enc.projectTo8(enc.encode(buf));
        if (cosineSimilarity(a, b) > 0.99f) {
            std::cout << "[PASS] Identical sim>0.99\n"; pass++;
        } else {
            std::cout << "[FAIL] Identical images low similarity\n"; fail++;
        }
    }

    // Test 6: Determinism — same input same output
    {
        std::vector<uint8_t> img(32 * 32 * 3, 200);
        ImageBuffer buf{img.data(), 32, 32, 3};
        VisualEncoder enc;
        auto a = enc.encode(buf);
        auto b = enc.encode(buf);
        bool same = true;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::fabs(a[i] - b[i]) > 1e-6f) same = false;
        if (same) { std::cout << "[PASS] Deterministic\n"; pass++; }
        else { std::cout << "[FAIL] Non-deterministic\n"; fail++; }
    }

    std::cout << "\n=== RESULT: " << pass << "/6 PASS ===\n";
    return (fail == 0) ? 0 : 1;
}
