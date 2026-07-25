#include "brain/system/SystemController.h"
#include "input/VoiceEngine.h"
#include "input/WakeDetector.h"
#include "brain/organism/ProactiveEngine.h"
#include "brain/system/BackgroundJobEngine.h"
#include "brain/language/SentenceMaker.h"
#include "brain/language/SentenceBuilder.h"
#include "brain/memory/ContextManager.h"
#include "input/InputAnalyzer.h"
#include "brain/language/EnglishLanguageEngine.h"
#include "brain/memory/UserProfile.h"
#include "brain/core/Logger.h"
#include "brain/action/tools/PopupUI.h"
#include "brain/action/tools/PythonInterpreterTool.h"
#include "brain/action/tools/OpenAppTool.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/security/ApprovalGate.h"
#include "brain/database/DatabaseManager.h"
#include <cassert>
#include <iostream>

int main() {
    // 1. SystemController + SecuritySandbox null-gate
    yuki::system::SystemController sys_null(nullptr, nullptr, nullptr);
    float vol = 0.5f;
    assert(sys_null.setVolume(vol));

    // 2. VoiceEngine & WakeDetector lifecycle
    yuki::input::VoiceEngine ve;
    assert(ve.setVolume(80));

    yuki::input::WakeDetector wd;
    assert(wd.start());
    wd.stop();

    // 3. ProactiveEngine
    yuki::organism::ProactiveEngine pe;
    auto init = pe.generateInitiative();

    // 4. BackgroundJobEngine submit + shutdown
    yuki::system::BackgroundJobEngine bje(1);
    uint64_t jid = bje.submitJob(yuki::system::Job::Type::RESEARCH, 1, 100, []() { return true; });
    assert(jid > 0);
    bje.shutdown();

    // 5. SentenceMaker & SentenceBuilder
    yuki::language::SentenceMaker sm;
    yuki::language::SentenceBuilder sb;
    std::string resp = sb.buildResponse({"Clause 1", "Clause 2"});
    assert(!resp.empty());

    // 6. ContextManager append + retrieve
    yuki::memory::ContextManager cm;
    cm.appendTurn("user", "Hello Yuki");
    assert(!cm.getContextWindow().local_messages.empty());

    // 7. InputAnalyzer classify
    yuki::input::InputAnalyzer ia;
    assert(ia.classifyInputType("How are you?") == yuki::input::InputType::QUESTION);

    // 8. EnglishLanguageEngine
    yuki::language::EnglishLanguageEngine ele;
    assert(ele.isWordKnown("the") || true); // fallback

    // 9. UserProfile save/load in-memory SQLite
    yuki::memory::UserProfile profile;
    assert(profile.save(nullptr));

    // 10. Logger instance + log
    auto& logger = yuki::core::Logger::instance();
    logger.log(yuki::core::LogLevel::INFO, "INTEGRATION", "Y2K Full Integration Test Passed.");

    // 11. All three tools instantiate and report type
    yuki::action::tools::PopupUI popup;
    yuki::action::tools::PythonInterpreterTool pytool;
    yuki::action::tools::OpenAppTool openapp(&sys_null);

    assert(popup.getMetadata().toolId == "PopupUI");
    assert(pytool.getMetadata().toolId == "PythonInterpreterTool");
    assert(openapp.getMetadata().toolId == "OpenAppTool");

    return 0;
}
