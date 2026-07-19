#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>

namespace yuki::memory { class CognitiveMemoryFabric; }
namespace yuki::perception { class TextEncoder; }
namespace yuki::inference { class VariationalStateEstimator; }
class KnowledgeDaemon;  // forward decl — avoid heavy include in header

namespace yuki::learning {

struct LearningSample {
    std::string text;
    std::string source;     // "curriculum", "user_turn", "web", "synthetic"
    std::string topic;
    std::vector<float> features; // 8 heuristic scores
    float label_confidence = 0.0f;
    std::chrono::system_clock::time_point timestamp;
};

class BackgroundLearningEngine {
public:
    BackgroundLearningEngine();
    ~BackgroundLearningEngine();

    void init(std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf,
              std::shared_ptr<yuki::perception::TextEncoder> encoder,
              yuki::inference::VariationalStateEstimator* vse);

    void start();
    void stop();

    void ingest(LearningSample sample);
    void ingestUserTurn(const std::string& text);
    void setCurriculumTopics(const std::vector<std::string>& topics);

    // Optional: wire the KnowledgeDaemon to drain its web-packet queue
    // during BLE's background processing loop.
    void setKnowledgeDaemon(KnowledgeDaemon* kd) { knowledge_daemon_ = kd; }

    uint64_t totalSamplesProcessed() const { return sample_count_.load(); }
    bool isRunning() const { return running_.load(); }

private:
    void loop();
    void processSample(const LearningSample& sample);
    LearningSample generateCurriculumSample();
    void injectSyntheticVseObservation();

    std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf_;
    std::shared_ptr<yuki::perception::TextEncoder> encoder_;
    yuki::inference::VariationalStateEstimator* vse_ = nullptr;
    KnowledgeDaemon* knowledge_daemon_ = nullptr;

    std::vector<std::string> curriculum_topics_;
    std::atomic<size_t> curriculum_index_{0};

    std::queue<LearningSample> queue_;
    std::mutex queue_mtx_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::atomic<uint64_t> sample_count_{0};
    std::atomic<uint64_t> synthetic_counter_{0};
};

} // namespace yuki::learning
