#include "brain/action/tools/OpenAppTool.h"
#include "brain/system/SystemController.h"
#include <string>

namespace yuki::action::tools {

OpenAppTool::OpenAppTool(yuki::system::SystemController* sys)
    : sys_(sys) {}

yuki::research::ToolMetadata OpenAppTool::getMetadata() const {
    yuki::research::ToolMetadata meta;
    meta.toolId = "OpenAppTool";
    meta.reliability = 0.90f;
    meta.cost = 2;
    meta.riskLevel = yuki::research::ToolRiskLevel::MEDIUM;
    return meta;
}

yuki::research::ToolResult OpenAppTool::execute(const std::vector<uint8_t>& input) {
    yuki::research::ToolResult res;
    std::string app_name(input.begin(), input.end());

    if (sys_) {
        std::string err;
        bool ok = sys_->openApplication(app_name, err);
        if (!ok) {
            res.status = yuki::research::ToolStatus::PERMISSION_DENIED;
            res.confidence = 0.0f;
            return res;
        }
    }

    res.status = yuki::research::ToolStatus::SUCCESS;
    res.confidence = 1.0f;
    res.payload = {'O', 'K'};
    return res;
}

} // namespace yuki::action::tools
