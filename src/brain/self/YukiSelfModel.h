#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>
#include <algorithm>

namespace yuki::gw { struct Message; }

namespace yuki::self {

struct DomainExpertise {
    std::string topic;
    float expertise_score = 0.0f;
    float confidence      = 0.5f;
    uint64_t interaction_count = 0;
    uint64_t success_count     = 0;
    std::chrono::system_clock::time_point last_updated;
    bool is_gap = false;
};

class YukiSelfModel {
public:
    YukiSelfModel();
    ~YukiSelfModel();

    void init(const std::string& db_path = "data/brain/self_model");
    void subscribeToBus();
    void start();
    void stop();

    std::string getSelfSummary() const;
    std::vector<DomainExpertise> getGaps()      const;
    std::vector<DomainExpertise> getStrengths() const;
    float getExpertise(const std::string& topic)  const;
    float getConfidence(const std::string& topic) const;
    bool  shouldLearn(const std::string& topic)   const;

    void recordInteraction(const std::string& topic, bool success, float confidence);
    void recordBeliefEntropy(float entropy);

private:
    void onActionCompleted(const yuki::gw::Message& msg);
    void onBeliefUpdate(const yuki::gw::Message& msg);
    void onUserTurn(const yuki::gw::Message& msg);
    void saveLoop();
    void saveToDb();
    void loadFromDb();

    std::unordered_map<std::string, DomainExpertise> domains_;
    mutable std::mutex mtx_;
    std::string db_path_;
    std::atomic<bool> running_{false};
    std::thread save_thread_;
    std::atomic<bool> dirty_{false};
};

} // namespace yuki::self
