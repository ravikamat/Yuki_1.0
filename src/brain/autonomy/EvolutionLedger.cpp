#include "src/brain/autonomy/EvolutionLedger.h"


namespace yuki::autonomy {

void EvolutionLedger::recordEvent(const EvolutionEvent& event) {
    events_.push_back(event);
}

std::vector<EvolutionEvent> EvolutionLedger::getEventsByCategory(const std::string& category) const {
    std::vector<EvolutionEvent> result;
    for (const auto& ev : events_) {
        if (ev.category == category) {
            result.push_back(ev);
        }
    }
    return result;
}

const std::vector<EvolutionEvent>& EvolutionLedger::allEvents() const noexcept {
    return events_;
}

} // namespace yuki::autonomy
