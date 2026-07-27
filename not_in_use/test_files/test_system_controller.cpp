#include "brain/system/SystemController.h"
#include "brain/security/SecuritySandbox.h"
#include "brain/security/ApprovalGate.h"
#include "brain/system/ResourceMonitor.h"
#include <cassert>
#include <string>

int main() {
    auto& sandbox = yuki::security::SecuritySandbox::instance();
    yuki::security::ApprovalGate gate(0.50f);
    yuki::system::ResourceMonitor monitor;
    yuki::system::SystemController sys(&sandbox, &gate, &monitor);

    // 1. screenshot() gated by SecuritySandbox (invalid path -> denied)
    std::string err;
    bool ok_shot = sys.screenshot("../../../invalid_path.bmp", err);
    assert(!ok_shot);
    assert(!err.empty());

    // 2. openUrl() rejects javascript: and file:// protocols
    bool ok_js = sys.openUrl("javascript:alert(1)", err);
    assert(!ok_js);
    bool ok_file = sys.openUrl("file:///C:/test.txt", err);
    assert(!ok_file);

    // 3. openApplication() requires ApprovalGate for unknown apps
    bool ok_app = sys.openApplication("unknown_app_123.exe", err);
    assert(!ok_app);

    // 4. getMetricsSnapshot() returns values in [0,1]
    auto snap = sys.getMetricsSnapshot();
    assert(snap.cpu_percent >= 0.0f && snap.cpu_percent <= 1.0f);
    assert(snap.ram_percent >= 0.0f && snap.ram_percent <= 1.0f);
    assert(snap.disk_percent >= 0.0f && snap.disk_percent <= 1.0f);
    assert(snap.volume_level >= 0.0f && snap.volume_level <= 1.0f);

    // 5. clipboard roundtrip (set -> get) works
    sys.setClipboardText("YUKI_TEST_CLIPBOARD");
    std::string clip_out;
    sys.getClipboardText(clip_out);
    assert(clip_out == "YUKI_TEST_CLIPBOARD");

    // 6. volume set/get consistency
    sys.setVolume(0.75f);
    assert(sys.getMetricsSnapshot().volume_level == 0.75f);
    sys.mute();
    assert(sys.getMetricsSnapshot().volume_level == 0.0f);
    sys.unmute();
    assert(sys.getMetricsSnapshot().volume_level == 0.75f);

    return 0;
}
