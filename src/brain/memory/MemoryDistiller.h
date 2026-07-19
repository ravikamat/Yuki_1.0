#pragma once
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace yuki::memory {
class CognitiveMemoryFabric;
struct EpisodeRecord;
}
namespace yuki::inference { class VariationalStateEstimator; }

namespace yuki::memory {

class MemoryDistiller {
public:
    MemoryDistiller();
    ~MemoryDistiller();

    // vse is optional (can pass nullptr)
    void init(std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf,
              yuki::inference::VariationalStateEstimator* vse = nullptr);

    void start();
    void stop();

    // Call on every user turn to reset the idle timer
    void bumpActivity();

    // GW SYSTEM_STATE subscriber hook
    void onSystemState(const std::string& state_json);

    bool     isConsolidating()         const { return consolidating_.load(); }
    uint64_t totalEpisodesConsolidated() const { return episodes_consolidated_.load(); }
    uint64_t totalCounterfactuals()      const { return counterfactuals_run_.load(); }

private:
    void sleepLoop();
    void runConsolidationPass();
    void patternSeparation(const std::vector<EpisodeRecord>& episodes);
    void patternCompletion();
    void counterfactualReplay(const std::vector<EpisodeRecord>& episodes);
    void precisionRecalibration(const std::vector<EpisodeRecord>& episodes);
    void lshRehashing();

    std::shared_ptr<yuki::memory::CognitiveMemoryFabric> cmf_;
    yuki::inference::VariationalStateEstimator* vse_ = nullptr;

    std::atomic<bool>     running_{false};
    std::atomic<bool>     consolidating_{false};
    std::atomic<bool>     sleep_requested_{false};
    std::thread           thread_;

    std::atomic<uint64_t> episodes_consolidated_{0};
    std::atomic<uint64_t> counterfactuals_run_{0};
    std::chrono::steady_clock::time_point last_activity_;
};

} // namespace yuki::memory
