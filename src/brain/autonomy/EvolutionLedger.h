#pragma once

#include "src/brain/autonomy/AutonomyTypes.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace yuki::autonomy {

class EvolutionLedger {
public:
    void recordEvent(const EvolutionEvent& event);
    std::vector<EvolutionEvent> getEventsByCategory(const std::string& category) const;
    const std::vector<EvolutionEvent>& allEvents() const noexcept;

private:
    std::vector<EvolutionEvent> events_;
};

} // namespace yuki::autonomy
