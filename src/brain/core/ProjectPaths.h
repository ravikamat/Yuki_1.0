#pragma once
// ProjectPaths.h — single source of truth for machine-specific locations.
//
// Resolves the Yuki project root without hardcoding a drive letter (audit §5:
// "F:/Yuki_1.0/..." was baked into six call sites). Precedence:
//
//   1. YUKI_ROOT environment variable (explicit override, any machine).
//   2. Walk up from the current working directory looking for a repo marker
//      (VERSION + CMakeLists.txt). Works when launched from the repo root or
//      from build-ninja/bin.
//   3. YUKI_SOURCE_DIR compile definition, set by CMake to the configured
//      source tree.
//   4. The current working directory, as a last resort.

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace yuki {
namespace paths {

// True when `dir` looks like the Yuki repository root.
inline bool is_project_root(const std::filesystem::path& dir) {
    std::error_code ec;
    return std::filesystem::exists(dir / "VERSION", ec) &&
           std::filesystem::exists(dir / "CMakeLists.txt", ec);
}

// Absolute path to the project root. Never returns an empty path.
inline std::filesystem::path projectRoot() {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (const char* env = std::getenv("YUKI_ROOT")) {
        if (*env) {
            fs::path candidate(env);
            if (fs::exists(candidate, ec)) return fs::absolute(candidate, ec);
        }
    }

    fs::path dir = fs::current_path(ec);
    if (ec) dir.clear();
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        if (is_project_root(dir)) return dir;
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }

#ifdef YUKI_SOURCE_DIR
    {
        fs::path candidate(YUKI_SOURCE_DIR);
        if (fs::exists(candidate, ec)) return fs::absolute(candidate, ec);
    }
#endif

    return fs::current_path(ec);
}

// Path under <root>/data, e.g. dataPath("models/yuki.gguf").
inline std::filesystem::path dataPath(const std::string& relative) {
    return projectRoot() / "data" / relative;
}

// Path under <root>/scripts, e.g. scriptsPath("openie_extractor.py").
inline std::filesystem::path scriptsPath(const std::string& relative) {
    return projectRoot() / "scripts" / relative;
}

// Forward-slash form, safe to embed in shell command strings on Windows.
inline std::string generic(const std::filesystem::path& path) {
    return path.generic_string();
}

} // namespace paths
} // namespace yuki
