#pragma once
#include <string>
#include <mutex>
#include <vector>

namespace yuki::security {

class ApprovalGate {
public:
    explicit ApprovalGate(float default_threshold = 0.50f);

    bool requestApproval(const std::string& action_description, float risk_score);
    bool evaluateOwnerDecision(bool requiresApproval, float riskScore);
    void setThreshold(float threshold);
    float getThreshold() const;


private:
    float threshold_{0.50f};
    mutable std::mutex mutex_;
};

} // namespace yuki::security
