#include "brain/action/tools/PopupUI.h"
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace yuki::action::tools {

yuki::research::ToolMetadata PopupUI::getMetadata() const {
    yuki::research::ToolMetadata meta;
    meta.toolId = "PopupUI";
    meta.reliability = 0.99f;
    meta.cost = 1;
    meta.riskLevel = yuki::research::ToolRiskLevel::LOW;
    return meta;
}

yuki::research::ToolResult PopupUI::execute(const std::vector<uint8_t>& input) {
    yuki::research::ToolResult res;
    std::string text(input.begin(), input.end());
    if (text.empty()) text = "Notification";

#ifdef _WIN32
    MessageBoxA(NULL, text.c_str(), "YUKI Notification", MB_OK | MB_ICONINFORMATION);
#endif

    res.status = yuki::research::ToolStatus::SUCCESS;
    res.confidence = 1.0f;
    res.payload = {'O', 'K'};
    return res;
}

} // namespace yuki::action::tools
