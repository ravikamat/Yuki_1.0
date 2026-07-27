#include "brain/action/tools/PopupUI.h"
#include "brain/action/tools/PythonInterpreterTool.h"
#include "brain/action/tools/OpenAppTool.h"
#include "brain/research/core/ToolRegistry.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/security/ApprovalGate.h"
#include "brain/system/SystemController.h"
#include <cassert>

int main() {
    yuki::research::ToolRegistry registry;
    auto& sandbox = yuki::security::SecuritySandbox::instance();
    yuki::security::ApprovalGate gate(0.50f);
    yuki::system::SystemController sys(&sandbox, &gate, nullptr);

    auto popup = std::make_shared<yuki::action::tools::PopupUI>();
    auto pytool = std::make_shared<yuki::action::tools::PythonInterpreterTool>(&sandbox, &gate);
    auto apptool = std::make_shared<yuki::action::tools::OpenAppTool>(&sys);

    // 1. PopupUI registered in ToolRegistry
    registry.registerActionTool(popup);
    assert(registry.hasActionTool("PopupUI"));

    // 2. PythonInterpreterTool registered and execute() returns ToolResult
    registry.registerActionTool(pytool);
    assert(registry.hasActionTool("PythonInterpreterTool"));
    std::string script = "print('hello')";
    auto res_py = pytool->execute(std::vector<uint8_t>(script.begin(), script.end()));
    assert(res_py.status == yuki::research::ToolStatus::SUCCESS || res_py.status == yuki::research::ToolStatus::PERMISSION_DENIED);

    // 3. OpenAppTool registered
    registry.registerActionTool(apptool);
    assert(registry.hasActionTool("OpenAppTool"));

    // 4. PythonInterpreterTool sandbox validation
    std::string bad_script = "import os; os.system('rm -rf /')";
    auto res_bad = pytool->execute(std::vector<uint8_t>(bad_script.begin(), bad_script.end()));
    assert(!res_bad.isSuccess() || res_bad.status == yuki::research::ToolStatus::PERMISSION_DENIED || res_bad.status == yuki::research::ToolStatus::SANDBOX_VIOLATION);

    // 5. ToolResult success/failure captured correctly
    auto res_popup = popup->execute({'T', 'e', 's', 't'});
    assert(res_popup.isSuccess());

    // 6. CapabilityProfile in ToolRegistry
    yuki::capability::CapabilityProfile prof;
    prof.tool_id = "UI_NOTIFY";
    registry.registerToolWithProfile("PopupUI", prof);

    auto p_opt = registry.getProfile("PopupUI");
    assert(p_opt.has_value());

    return 0;
}
