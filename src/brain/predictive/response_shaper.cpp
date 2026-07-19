// =============================================================================
// yuki/core/response_shaper.cpp
// ResponseShaper::profile_from_belief() and apply()
// Rule 19: profile flags from BeliefPool; apply modifies base response string
// =============================================================================

#include "predictive_turn_engine.h"
#include "../core/ResponseResolver.h"
#include <algorithm>

namespace yuki {

ResponseShaper::ToneProfile
ResponseShaper::profile_from_belief(const BeliefPool& pool) {
    ToneProfile p;
    float tone_mass   = pool.belief_mass("tone");
    float entity_mass = pool.belief_mass("entity");
    float intent_mass = pool.belief_mass("intent");

    // High emotional tone → acknowledge first, shorten
    if (tone_mass > 0.60f) {
        p.acknowledge_first = true;
        p.shorten           = true;
    }

    // Vague entity → suppress detail (don't hallucinate specifics)
    if (entity_mass < 0.30f)
        p.suppress_detail = true;

    // High intent confidence → expand with confident delivery
    if (intent_mass > 0.80f)
        p.expand_detail = true;

    return p;
}

std::string ResponseShaper::apply(const std::string& base_response,
                                   const ToneProfile& profile,
                                   const PrecisionState& /*precision*/)
{
    std::string result = base_response;

    if (profile.acknowledge_first)
        result = ResponseResolver::instance().resolve("shaper.acknowledge") + result;

    if (profile.suppress_detail) {
        // Truncate overly detailed responses
        if (result.size() > 60)
            result = result.substr(0, 60);
    } else if (profile.shorten && result.size() > 100) {
        result = result.substr(0, 100);
    }

    if (profile.expand_detail)
        result += ResponseResolver::instance().resolve("shaper.high_confidence");

    return result;
}

} // namespace yuki
