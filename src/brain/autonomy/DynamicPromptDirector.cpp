#include "src/brain/autonomy/DynamicPromptDirector.h"
#include "src/brain/core/ConfigManager.h"

namespace yuki::autonomy {

std::string DynamicPromptDirector::buildSystemPrompt(const std::string& mode,
                                                      const std::string& ownerIntent,
                                                      const std::string& backend,
                                                      float risk,
                                                      float budget) const {
    std::string basePrompt = ConfigManager::instance().loadPromptTemplate("autonomy_system");
    if (basePrompt.empty()) {
        basePrompt = "You are the language cortex of YUKI, a persistent digital organism.\n"
                     "Prioritize owner intent, evidence, safe execution, explicit planning, and verification.";
    }

    std::string modeTemplate;
    if (mode == "research") {
        modeTemplate = ConfigManager::instance().loadPromptTemplate("research_mode");
    } else if (mode == "code_synthesis") {
        modeTemplate = ConfigManager::instance().loadPromptTemplate("code_synthesis_mode");
    } else if (mode == "self_critique") {
        modeTemplate = ConfigManager::instance().loadPromptTemplate("self_critique_mode");
    } else if (mode == "watchdog_review") {
        modeTemplate = ConfigManager::instance().loadPromptTemplate("watchdog_review_mode");
    }

    std::string result = basePrompt + "\n\n";
    if (!modeTemplate.empty()) {
        result += "[Mode Spec]\n" + modeTemplate + "\n\n";
    }
    result += "[Context & Constraints]\n";
    result += "Owner Intent: " + ownerIntent + "\n";
    result += "Backend: " + backend + "\n";
    result += "Risk Level: " + std::to_string(risk) + "\n";
    result += "Budget Limit: " + std::to_string(budget) + "\n";

    return result;
}

} // namespace yuki::autonomy
