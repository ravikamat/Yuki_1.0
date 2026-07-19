#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include "input/encoding/TextEncoder.h"

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

// Legacy Test 1: Vocabulary build
TEST(TextEncoder, VocabBuild) {
    TextEncoder enc;
    std::vector<std::string> corpus;
    for (int i = 0; i < 50; ++i)
        corpus.push_back("the quick brown fox jumps over the lazy dog number " + std::to_string(i));
    enc.buildVocabulary(corpus);
    EXPECT_GT(enc.vocabSize(), 20U);
}

// Legacy Test 2: encode returns 64D finite values
TEST(TextEncoder, Encode64DFinite) {
    TextEncoder enc;
    enc.seedCurriculumVocabulary({"hello world test"});
    auto vec = enc.encode("hello world");
    ASSERT_EQ(vec.size(), 64U);
    bool ok = true;
    for (float v : vec) {
        if (std::isnan(v) || std::isinf(v)) ok = false;
    }
    EXPECT_TRUE(ok);
}

// Legacy Test 3: projectTo8 output is L2-normalized
TEST(TextEncoder, ProjectTo8Norm) {
    TextEncoder enc;
    std::vector<float> dummy(64, 0.5f);
    auto proj = enc.projectTo8(dummy);
    float sq = 0.0f;
    for (float v : proj) sq += v * v;
    EXPECT_TRUE(approxEqual(std::sqrt(sq), 1.0f, 0.01f));
}

// Legacy Test 4: Semantic similarity — king~queen > king~apple
TEST(TextEncoder, SemanticSimilarity) {
    TextEncoder enc;
    std::vector<std::string> corpus;
    for (int i = 0; i < 100; ++i) {
        corpus.push_back("king queen royal crown palace throne monarch");
        corpus.push_back("apple banana fruit orange grape juice");
    }
    enc.buildVocabulary(corpus);
    for (int epoch = 0; epoch < 10; ++epoch) {
        for (const auto& s : corpus) enc.trainStep(s, 0.01f);
    }
    auto v_king  = enc.encode("king");
    auto v_queen = enc.encode("queen");
    auto v_apple = enc.encode("apple");
    EXPECT_GT(cosineSimilarity(v_king, v_queen), cosineSimilarity(v_king, v_apple));
}

// Legacy Test 5: JL variance — projection preserves cosine similarity (rough)
TEST(TextEncoder, JLCosinePreservation) {
    TextEncoder enc;
    float avg_err = 0.0f;
    std::srand(123);
    for (int t = 0; t < 100; ++t) {
        std::vector<float> a(64), b(64);
        float na = 0.0f, nb = 0.0f;
        for (int i = 0; i < 64; ++i) {
            a[static_cast<size_t>(i)] = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) - 0.5f;
            b[static_cast<size_t>(i)] = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) - 0.5f;
            na += a[static_cast<size_t>(i)] * a[static_cast<size_t>(i)];
            nb += b[static_cast<size_t>(i)] * b[static_cast<size_t>(i)];
        }
        // Cosine similarity in 64D
        float dot64 = 0.0f;
        for (int i = 0; i < 64; ++i) dot64 += a[static_cast<size_t>(i)] * b[static_cast<size_t>(i)];
        float cos64 = dot64 / (std::sqrt(na) * std::sqrt(nb) + 1e-8f);

        // Cosine similarity in projected 8D
        auto pa = enc.projectTo8(a);
        auto pb = enc.projectTo8(b);
        float cos8 = cosineSimilarity(pa, pb);

        avg_err += std::fabs(cos64 - cos8);
    }
    avg_err /= 100.0f;
    EXPECT_LT(avg_err, 0.40f);
}

// Legacy Test 6: OOV input → zero vector (except heuristic positions 0..8 which are rule-based and not OOV-based)
TEST(TextEncoder, OOVZeroVector) {
    TextEncoder enc;
    auto vec = enc.encode("completely unknown xyz123");
    ASSERT_EQ(vec.size(), 64U);
    bool all_zero = true;
    for (size_t i = 9; i < 64; ++i) {
        if (vec[i] != 0.0f) all_zero = false;
    }
    EXPECT_TRUE(all_zero);
}

TEST(TextEncoder, PhaticScore) {
    TextEncoder enc;
    enc.encode("yes");
    auto s = enc.getLastScores();
    EXPECT_GT(s.phatic, 0.8f);
    EXPECT_LT(s.question, 0.3f);
    
    enc.encode("I am Ravi, your friend");
    s = enc.getLastScores();
    EXPECT_GT(s.phatic, 0.8f);
}

TEST(TextEncoder, QuestionScore) {
    TextEncoder enc;
    enc.encode("What is 2+2?");
    auto s = enc.getLastScores();
    EXPECT_GT(s.question, 0.7f);
    EXPECT_LT(s.phatic, 0.3f);
    
    enc.encode("Tell me about Python");
    s = enc.getLastScores();
    EXPECT_GT(s.question, 0.6f);
}

TEST(TextEncoder, PhaticAndQuestionAreDistinct) {
    TextEncoder enc;
    enc.encode("What is your name?");
    auto s = enc.getLastScores();
    // "What is" triggers question, "your name" is not phatic
    EXPECT_GT(s.question, 0.5f);
    EXPECT_LT(s.phatic, 0.3f);
}
