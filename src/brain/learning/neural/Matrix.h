#pragma once
#include <vector>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <iostream>

namespace yuki::learning::neural {

class Matrix {
public:
    size_t rows{0};
    size_t cols{0};
    std::vector<float> data;

    Matrix() = default;
    Matrix(size_t r, size_t c, float initial_val = 0.0f);
    Matrix(size_t r, size_t c, const std::vector<float>& d);

    static Matrix random(size_t r, size_t c, float min_val = -1.0f, float max_val = 1.0f);
    static Matrix zeros(size_t r, size_t c);
    static Matrix ones(size_t r, size_t c);
    static Matrix identity(size_t n);

    float& operator()(size_t r, size_t c);
    float operator()(size_t r, size_t c) const;

    Matrix transpose() const;
    Matrix multiply(const Matrix& other) const;
    Matrix add(const Matrix& other) const;
    Matrix subtract(const Matrix& other) const;
    Matrix elementwise_multiply(const Matrix& other) const;

    Matrix scalar_multiply(float scalar) const;
    Matrix scalar_add(float scalar) const;

    Matrix apply(const std::function<float(float)>& func) const;

    std::vector<uint8_t> serialize() const;
    static Matrix deserialize(const std::vector<uint8_t>& bytes);
};

} // namespace yuki::learning::neural
