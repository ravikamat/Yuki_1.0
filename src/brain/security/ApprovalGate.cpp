#include "brain/security/ApprovalGate.h"

namespace yuki::security {

ApprovalGate::ApprovalGate(float default_threshold)
    : threshold_(default_threshold) {}

bool ApprovalGate::requestApproval(const std::string& /*action_description*/, float risk_score) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Auto-approve if risk <= threshold
    return (risk_score <= threshold_);
}

void ApprovalGate::setThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    threshold_ = threshold;
}

float ApprovalGate::getThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return threshold_;
}

} // namespace yuki::security
