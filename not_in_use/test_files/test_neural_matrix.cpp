#include <gtest/gtest.h>
#include "brain/learning/neural/Matrix.h"

using namespace yuki::learning::neural;

TEST(NeuralMatrixTest, BasicOperations) {
    Matrix a(2, 2, {1.0f, 2.0f, 3.0f, 4.0f});
    Matrix b(2, 2, {5.0f, 6.0f, 7.0f, 8.0f});

    Matrix c = a.add(b);
    EXPECT_FLOAT_EQ(c(0, 0), 6.0f);
    EXPECT_FLOAT_EQ(c(1, 1), 12.0f);

    Matrix d = a.multiply(b);
    EXPECT_FLOAT_EQ(d(0, 0), 19.0f); // 1*5 + 2*7 = 19
    EXPECT_FLOAT_EQ(d(0, 1), 22.0f); // 1*6 + 2*8 = 22

    Matrix t = a.transpose();
    EXPECT_FLOAT_EQ(t(0, 1), 3.0f);
    EXPECT_FLOAT_EQ(t(1, 0), 2.0f);
}

TEST(NeuralMatrixTest, Serialization) {
    Matrix m = Matrix::random(3, 4, -1.0f, 1.0f);
    auto bytes = m.serialize();
    EXPECT_FALSE(bytes.empty());

    Matrix restored = Matrix::deserialize(bytes);
    EXPECT_EQ(restored.rows, 3u);
    EXPECT_EQ(restored.cols, 4u);
    for (size_t i = 0; i < m.data.size(); ++i) {
        EXPECT_FLOAT_EQ(m.data[i], restored.data[i]);
    }
}
