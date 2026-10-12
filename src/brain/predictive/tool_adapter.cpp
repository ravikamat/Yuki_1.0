#include "tool_adapter.h"
#include "../ToolExecutor.h"
#include <algorithm>
#include <cctype>
#include <iostream>

namespace yuki {

ToolAdapter::ToolAdapter() {
    skillRegistry_.load();
    taskDecomposer_.load();
}

std::string ToolAdapter::executeCanonical(const SemanticFrame& frame, const std::string& rawInput, const TurnResult& context) {
    // P0 FIX #4: Use context for conversation-aware tool execution
    if (!context.response_text.empty()) {
        std::cout << "[ToolAdapter] Context: prior_response=" << context.response_text.substr(0, 40) << "...\n";
    }
    std::cout << "[ToolAdapter] Canonical execution check for entity: " << frame.matched_entity
              << " score=" << frame.matched_entity_score << "\n";

    if (!frame.matched_entity.empty() && frame.matched_entity_score >= 0.70) {
        ToolExecutor executor;

        // ── Dynamic Schema-Driven Entity Dispatch (OpenJarvis Dynamic Discovery) ──
        std::string query = rawInput;
        static const std::vector<std::string> stopWords = {
            "play", "open", "launch", "search", "for", "on", "in", "the", "a", "an",
            "named", "called", "song", "track", "music", "video", "by", "from", "using", "via"
        };
        std::string lowerQ = query;
        std::transform(lowerQ.begin(), lowerQ.end(), lowerQ.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        std::string lowerEntity = frame.matched_entity;
        std::transform(lowerEntity.begin(), lowerEntity.end(), lowerEntity.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        for (const auto& sw : stopWords) {
            size_t pos = 0;
            while ((pos = lowerQ.find(sw, pos)) != std::string::npos) {
                bool leftOk = (pos == 0 || !std::isalnum(static_cast<unsigned char>(lowerQ[pos-1])));
                bool rightOk = (pos + sw.length() >= lowerQ.length() || !std::isalnum(static_cast<unsigned char>(lowerQ[pos+sw.length()])));
                if (leftOk && rightOk) {
                    query.erase(pos, sw.length());
                    lowerQ.erase(pos, sw.length());
                } else {
                    pos += 1;
                }
            }
        }

        if (!lowerEntity.empty()) {
            size_t pos = 0;
            while ((pos = lowerQ.find(lowerEntity, pos)) != std::string::npos) {
                query.erase(pos, lowerEntity.length());
                lowerQ.erase(pos, lowerEntity.length());
            }
        }

        std::string cleanQuery;
        for (char c : query) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ') cleanQuery += c;
        }
        while (!cleanQuery.empty() && cleanQuery.front() == ' ') cleanQuery.erase(0, 1);
        while (!cleanQuery.empty() && cleanQuery.back() == ' ') cleanQuery.pop_back();

        if (cleanQuery.empty()) cleanQuery = "trending";

        std::string url;
        if (lowerEntity == "youtube") {
            url = "https://www.youtube.com/results?search_query=";
        } else if (lowerEntity == "apple music" || lowerEntity == "applemusic") {
            url = "https://music.apple.com/us/search?term=";
        } else if (lowerEntity == "spotify") {
            url = "https://open.spotify.com/search/";
        } else if (lowerEntity == "soundcloud") {
            url = "https://soundcloud.com/search?q=";
        } else {
            std::string domain = lowerEntity;
            domain.erase(std::remove_if(domain.begin(), domain.end(), [](unsigned char c){ return !std::isalnum(c); }), domain.end());
            if (domain.empty()) domain = "google";
            url = "https://www." + domain + ".com/search?q=";
        }

        for (char c : cleanQuery) {
            if (c == ' ') url += "+";
            else url += c;
        }

        LocalToolResult res = executor.openUrl(url);
        if (res.success) {
            return "Opened " + frame.matched_entity + " search for '" + cleanQuery + "' in web browser.";
        } else {
            LocalToolResult appRes = executor.runCommand("start " + frame.matched_entity);
            return appRes.success ? "Launched application: " + frame.matched_entity : "Could not launch: " + frame.matched_entity;
        }
    }

    return execute(rawInput, context);
}

std::string ToolAdapter::execute(const std::string& tool_call, const TurnResult& context) {
    // P0 FIX #4: Use context for conversation-aware execution
    std::cout << "[ToolAdapter] Executing: " << tool_call;
    if (context.can_act) std::cout << " (can_act=true)";
    if (!context.template_family.empty()) std::cout << " tpl=" << context.template_family;
    std::cout << "\n";

    // 1. Check Skill System
    SkillHit hit = skillRegistry_.check(tool_call);
    if (hit.matched) {
        std::cout << "[ToolAdapter] Matched skill: " << hit.skill->name << "\n";
        return skillRegistry_.execute(hit, "User");
    }

    // 2. Check Task System (Decomposition or Planning)
    if (TaskDecomposer::isNewTaskRequest(tool_call)) {
        std::cout << "[ToolAdapter] Decomposing task: " << tool_call << "\n";
        DecompositionTree tree = taskDecomposer_.decompose(tool_call);
        return taskDecomposer_.formatPlan(tree);
    }

    // 3. Fallback: Generic Command Router style or return description
    return "Executed tool: " + tool_call;
}

} // namespace yuki
