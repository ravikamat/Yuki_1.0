#pragma once
#include <memory>
#include <atomic>
#include <string>
#include <vector>

namespace yuki {
namespace memory { class CognitiveMemoryFabric; }

namespace learning {

struct CurriculumTopic {
    std::string name;
    std::string tag;
    std::vector<std::string> seed_urls;
    std::vector<std::string> local_files;
    size_t target_samples = 500;
};

class MassCurriculumLoader {
public:
    explicit MassCurriculumLoader(std::shared_ptr<memory::CognitiveMemoryFabric> cmf);
    ~MassCurriculumLoader();

    static bool isCompleted();
    void execute();
    double progress() const { return progress_.load(); }
    bool isRunning() const { return running_.load(); }

private:
    std::shared_ptr<memory::CognitiveMemoryFabric> cmf_;
    std::atomic<bool> running_{false};
    std::atomic<double> progress_{0.0};

    std::vector<CurriculumTopic> getTopics() const;
    void generateBootstrapDefaults();
    void ingestLocalFile(const std::string& path, const std::string& topic);
    void writeCompletionFlag();
};

} // namespace learning
} // namespace yuki
