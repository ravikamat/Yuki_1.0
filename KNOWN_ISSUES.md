# YUKI v1.0 — Active Known Issues & Bug Tracker
> **File Name:** `KNOWN_ISSUES.md`  
> **Last Updated:** 2026-07-22  
> **Master Audit Log:** [`issue_stb_heru.md`](file:///d:/Yuki_1.0/issue_stb_heru.md)  
> **Authoritative Flow Reference:** [`yuki_flow.md`](file:///d:/Yuki_1.0/yuki_flow.md)

---

## 1. Active Priority 0 (P0) Blockers

| Issue ID | Component | Description | Status | Workaround / Mitigation |
|:---|:---|:---|:---:|:---|
| `ISSUE-P0-01` | `ScreenRuntime.cpp` | Occasional GDI screen handle leak on high-frequency screen capture loop under Win32 GDI. | 🟡 OPEN | DXGI Desktop Duplication API fallback path enabled. |

---

## 2. Active Priority 1 (P1) High Priority Issues

| Issue ID | Component | Description | Status | Workaround / Mitigation |
|:---|:---|:---|:---:|:---|
| `ISSUE-P1-01` | `SpeechSystem.cpp` | WASAPI audio capture buffer overflow retry logic drops initial 50ms audio chunk on cold device start. | 🟡 OPEN | Pre-warm audio buffer during Stage 1 Bootstrapping. |

---

## 3. Active Priority 2 (P2) Medium Priority Issues

| Issue ID | Component | Description | Status | Workaround / Mitigation |
|:---|:---|:---|:---:|:---|
| `ISSUE-P2-01` | `PresenceShell.cpp` | GUI overlay acrylic transparency flickers during rapid window resize events. | 🟡 OPEN | Double-buffer GDI+ rendering context. |
| `ISSUE-P2-02` | `test_predictive_turn_engine` | Test mock timing jitter under low-core VM test runners. | 🟡 OPEN | Increase test assertion timeout tolerance from 50ms to 200ms. |

---

## 4. Master Audit Log Reference

For the comprehensive 27-item code audit log containing line numbers, code snippets, severity ratings, and exact remediation steps for all stubs, hardcoded values, and architectural gaps across `src/`, inspect [`issue_stb_heru.md`](file:///d:/Yuki_1.0/issue_stb_heru.md).

---
*End of `KNOWN_ISSUES.md`*
