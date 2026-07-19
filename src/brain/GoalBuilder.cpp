#include "GoalBuilder.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>

GoalBuilder::GoalBuilder() {}

// ── Local helper: strip question prefixes, return the topic phrase ──────────
// For comparison queries ("compare X and Y"), sets primary topic = X,
// secondary topic in parameters["compare_with"] = Y.
// For all others, strips the leading question word and returns the subject.
static std::string extractTopicOrTarget(const MeaningState& meaning,
                                         std::map<std::string, std::string>* params = nullptr) {
    // Prefer entities extracted by the pipeline
    if (!meaning.objects.empty() && !meaning.objects[0].empty())
        return meaning.objects[0];

    std::string q = meaning.language.normalizedEnglish;
    if (q.empty()) q = meaning.best_hypothesis;

    // Lowercase working copy for prefix stripping
    std::string lower = q;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // ── Comparison query: "compare X and Y" / "X vs Y" / "difference between X and Y" ──
    // Extract both topics when detected.
    auto extractComparison = [&](const std::string& after) -> std::string {
        // Try " and " separator
        size_t andPos = after.find(" and ");
        if (andPos != std::string::npos) {
            std::string primary   = after.substr(0, andPos);
            std::string secondary = after.substr(andPos + 5);
            // Strip trailing punctuation from both
            while (!primary.empty()   && (primary.back()   == '?' || primary.back()   == ' ')) primary.pop_back();
            while (!secondary.empty() && (secondary.back() == '?' || secondary.back() == ' ')) secondary.pop_back();
            if (!primary.empty() && !secondary.empty() && params) {
                (*params)["compare_with"] = secondary;
            }
            return primary.empty() ? after : primary;
        }
        // Try " vs " separator
        size_t vsPos = after.find(" vs ");
        if (vsPos != std::string::npos) {
            std::string primary   = after.substr(0, vsPos);
            std::string secondary = after.substr(vsPos + 4);
            while (!primary.empty()   && primary.back()   == ' ') primary.pop_back();
            while (!secondary.empty() && secondary.back() == ' ') secondary.pop_back();
            if (!primary.empty() && !secondary.empty() && params) {
                (*params)["compare_with"] = secondary;
            }
            return primary.empty() ? after : primary;
        }
        return after;
    };

    // Check comparison-specific prefixes first
    static const struct { const char* pfx; bool isCompare; } PREFIXES[] = {
        // comparison prefixes (will call extractComparison)
        {"compare ",                true},
        {"difference between ",     true},
        {"difference of ",          true},
        {"similarities between ",   true},
        // research prefixes
        {"explain in depth ",       false},
        {"explain in detail ",      false},
        {"deep dive into ",         false},
        {"research ",               false},
        {"investigate ",            false},
        // standard question prefixes
        {"what is ",                false},
        {"what are ",               false},
        {"what does ",              false},
        {"what do ",                false},
        {"who is ",                 false},
        {"who are ",                false},
        {"how to ",                 false},
        {"how do i ",               false},
        {"how do you ",             false},
        {"how does ",               false},
        {"tell me about ",          false},
        {"explain ",                false},
        {"describe ",               false},
        {"i want to ",              false},
        {"i need to ",              false},
        {"i want a ",               false},
        {"build me a ",             false},
        {"build a ",                false},
        {"build ",                  false},
        {"make me a ",              false},
        {"make a ",                 false},
        {"make ",                   false},
        {"create a ",               false},
        {"create ",                 false},
        {"send a ",                 false},
        {"send ",                   false},
        {nullptr,                   false}
    };

    for (int i = 0; PREFIXES[i].pfx != nullptr; ++i) {
        const char* pfx  = PREFIXES[i].pfx;
        size_t      plen = strlen(pfx);
        if (lower.size() > plen && lower.compare(0, plen, pfx) == 0) {
            std::string remainder = q.substr(plen);
            // Strip trailing punctuation
            while (!remainder.empty() &&
                   (remainder.back() == '?' || remainder.back() == '.' ||
                    remainder.back() == '!' || remainder.back() == ' '))
                remainder.pop_back();

            if (remainder.size() < 2) continue;

            if (PREFIXES[i].isCompare) {
                std::string lowerRem = remainder;
                std::transform(lowerRem.begin(), lowerRem.end(), lowerRem.begin(),
                               [](unsigned char c){ return std::tolower(c); });
                return extractComparison(lowerRem);
            }
            return remainder;
        }
    }

    // "X vs Y" at top level (no leading prefix)
    {
        size_t vsPos = lower.find(" vs ");
        if (vsPos != std::string::npos && vsPos > 0) {
            std::string primary   = q.substr(0, vsPos);
            std::string secondary = q.substr(vsPos + 4);
            while (!primary.empty()   && primary.back()   == ' ') primary.pop_back();
            while (!secondary.empty() && secondary.back() == ' ') secondary.pop_back();
            if (primary.size() >= 2 && secondary.size() >= 2 && params) {
                (*params)["compare_with"] = secondary;
            }
            if (primary.size() >= 2) return primary;
        }
    }

    return "";  // caller will fall back to best_hypothesis
}

Goal GoalBuilder::build(const MeaningState& state) const {
    Goal g;
    g.action = state.request_type;
    g.risk   = Goal::RiskLevel::SAFE;

    // Always run extractTopicOrTarget to populate g.parameters (like compare_with)
    std::string extractedTarget = extractTopicOrTarget(state, &g.parameters);

    // Prefer using our newly classified entities if available
    if (!state.entities.empty() && !state.entities[0].canonical_form.empty()) {
        g.target = state.entities[0].canonical_form;
    } else {
        g.target = extractedTarget;
    }

    if (g.target.empty())
        g.target = state.best_hypothesis;

    return g;
}
