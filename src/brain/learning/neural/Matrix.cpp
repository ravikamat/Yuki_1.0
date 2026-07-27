#include "Matrix.h"
#include <random>
#include <cstring>
#include <cmath>

namespace yuki::learning::neural {

Matrix::Matrix(size_t r, size_t c, float initial_val)
    : rows(r), cols(c), data(r * c, initial_val) {}

Matrix::Matrix(size_t r, size_t c, const std::vector<float>& d)
    : rows(r), cols(c), data(d) {
    if (d.size() != r * c) {
        throw std::invalid_argument("Data size mismatch");
    }
}

Matrix Matrix::random(size_t r, size_t c, float min_val, float max_val) {
    Matrix m(r, c);
    static std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(min_val, max_val);
    for (size_t i = 0; i < r * c; ++i) {
        m.data[i] = dist(gen);
    }
    return m;
}

Matrix Matrix::zeros(size_t r, size_t c) {
    return Matrix(r, c, 0.0f);
}

Matrix Matrix::ones(size_t r, size_t c) {
    return Matrix(r, c, 1.0f);
}

Matrix Matrix::identity(size_t n) {
    Matrix m(n, n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        m(i, i) = 1.0f;
    }
    return m;
}

float& Matrix::operator()(size_t r, size_t c) {
    return data[r * cols + c];
}

float Matrix::operator()(size_t r, size_t c) const {
    return data[r * cols + c];
}

Matrix Matrix::transpose() const {
    Matrix result(cols, rows);
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            result(j, i) = (*this)(i, j);
        }
    }
    return result;
}

Matrix Matrix::multiply(const Matrix& other) const {
    if (cols != other.rows) {
        throw std::invalid_argument("Matrix dimension mismatch for multiplication");
    }
    Matrix result(rows, other.cols, 0.0f);
    for (size_t i = 0; i < rows; ++i) {
        for (size_t k = 0; k < cols; ++k) {
            float temp = (*this)(i, k);
            for (size_t j = 0; j < other.cols; ++j) {
                result(i, j) += temp * other(k, j);
            }
        }
    }
    return result;
}

Matrix Matrix::add(const Matrix& other) const {
    if (rows != other.rows || cols != other.cols) {
        throw std::invalid_argument("Matrix dimension mismatch for addition");
    }
    Matrix result(rows, cols);
    for (size_t i = 0; i < data.size(); ++i) {
        result.data[i] = data[i] + other.data[i];
    }
    return result;
}

Matrix Matrix::subtract(const Matrix& other) const {
    if (rows != other.rows || cols != other.cols) {
        throw std::invalid_argument("Matrix dimension mismatch for subtraction");
    }
    Matrix result(rows, cols);
    for (size_t i = 0; i < data.size(); ++i) {
        result.data[i] = data[i] - other.data[i];
    }
    return result;
}

Matrix Matrix::elementwise_multiply(const Matrix& other) const {
    if (rows != other.rows || cols != other.cols) {
        throw std::invalid_argument("Matrix dimension mismatch for elementwise multiplication");
    }
    Matrix result(rows, cols);
    for (size_t i = 0; i < data.size(); ++i) {
        result.data[i] = data[i] * other.data[i];
    }
    return result;
}

Matrix Matrix::scalar_multiply(float scalar) const {
    Matrix result(rows, cols);
    for (size_t i = 0; i < data.size(); ++i) {
        result.data[i] = data[i] * scalar;
    }
    return result;
}

Matrix Matrix::scalar_add(float scalar) const {
    Matrix result(rows, cols);
    for (size_t i = 0; i < data.size(); ++i) {
        result.data[i] = data[i] + scalar;
    }
    return result;
}

Matrix Matrix::apply(const std::function<float(float)>& func) const {
    Matrix result(rows, cols);
    for (size_t i = 0; i < data.size(); ++i) {
        result.data[i] = func(data[i]);
    }
    return result;
}

std::vector<uint8_t> Matrix::serialize() const {
    std::vector<uint8_t> bytes;
    uint64_t r = rows;
    uint64_t c = cols;
    uint64_t s = data.size();
    
    bytes.resize(sizeof(r) + sizeof(c) + sizeof(s) + s * sizeof(float));
    uint8_t* ptr = bytes.data();
    
    std::memcpy(ptr, &r, sizeof(r)); ptr += sizeof(r);
    std::memcpy(ptr, &c, sizeof(c)); ptr += sizeof(c);
    std::memcpy(ptr, &s, sizeof(s)); ptr += sizeof(s);
    if (s > 0) {
        std::memcpy(ptr, data.data(), s * sizeof(float));
    }
    return bytes;
}

Matrix Matrix::deserialize(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < sizeof(uint64_t) * 3) {
        throw std::invalid_argument("Invalid buffer size for Matrix deserialization");
    }
    uint64_t r, c, s;
    const uint8_t* ptr = bytes.data();
    
    std::memcpy(&r, ptr, sizeof(r)); ptr += sizeof(r);
    std::memcpy(&c, ptr, sizeof(c)); ptr += sizeof(c);
    std::memcpy(&s, ptr, sizeof(s)); ptr += sizeof(s);
    
    Matrix m(r, c);
    m.data.resize(s);
    if (s > 0) {
        std::memcpy(m.data.data(), ptr, s * sizeof(float));
    }
    return m;
}

} // namespace yuki::learning::neural
