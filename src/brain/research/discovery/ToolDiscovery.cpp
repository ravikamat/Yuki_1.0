#include "brain/research/discovery/ToolDiscovery.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace yuki {
namespace research {

// ---- Constants ----
static constexpr uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ULL;
static constexpr uint64_t kFnvPrime       = 0x100000001b3ULL;
static constexpr float    kBaseReliability = 0.85f;
static constexpr float    kHighReliability = 0.95f;
static constexpr float    kUnknownReliability = 0.50f;

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = kFnvOffsetBasis;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

// ---- PATH scanning ----

void ToolDiscovery::scanPathEnvironment() {
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return;

    std::string pathStr(pathEnv);
#ifdef _WIN32
    constexpr char kPathSep = ';';
    const std::vector<std::string> execExtensions = {".exe", ".cmd", ".bat", ".ps1"};
#else
    constexpr char kPathSep = ':';
    const std::vector<std::string> execExtensions = {""};
#endif

    // Split PATH into directories
    std::vector<std::string> pathDirs;
    std::string current;
    for (char c : pathStr) {
        if (c == kPathSep) {
            if (!current.empty()) { pathDirs.push_back(current); current.clear(); }
        } else {
            current += c;
        }
    }
    if (!current.empty()) pathDirs.push_back(current);

    // Scan each directory for executables
    for (const auto& dir : pathDirs) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec) || ec) continue;
        if (!std::filesystem::is_directory(dir, ec) || ec) continue;

        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec) || ec) continue;

            auto path = entry.path();
            std::string filename = path.filename().string();
            std::string ext = path.extension().string();

            // Check if it has an executable extension (or no extension on Unix)
            bool isExec = false;
            for (const auto& validExt : execExtensions) {
                if (validExt.empty() || ext == validExt) {
                    isExec = true;
                    break;
                }
            }
            if (!isExec) continue;

            // Deduplicate: check if we already discovered this tool
            std::string baseName = path.stem().string();
            uint64_t toolHash = fnv1a(baseName);
            bool duplicate = false;
            for (const auto& existing : discovered_) {
                if (fnv1a(existing.toolId) == toolHash) { duplicate = true; break; }
            }
            if (duplicate) continue;

            ToolMetadata meta = inferFromPath(path.string());
            meta.toolId = baseName;
            meta.schema.inputSchemaHash = toolHash;
            meta.schema.outputSchemaHash = toolHash ^ kFnvPrime;
            meta.reliability = isKnownExecutable(baseName) ? kHighReliability : kBaseReliability;
            meta.cost = 1;
            meta.riskLevel = ToolRiskLevel::LOW;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanPluginDirectories() {
    // Scan local plugin directories and common package manager locations
    std::vector<std::string> pluginDirs;

#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (appData) {
        pluginDirs.push_back(std::string(appData) + "\\yuki\\plugins");
    }
    if (localAppData) {
        pluginDirs.push_back(std::string(localAppData) + "\\yuki\\plugins");
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        pluginDirs.push_back(std::string(home) + "/.yuki/plugins");
        pluginDirs.push_back(std::string(home) + "/.local/share/yuki/plugins");
    }
    pluginDirs.push_back("/usr/share/yuki/plugins");
#endif

    for (const auto& dir : pluginDirs) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec) || ec) continue;
        if (!std::filesystem::is_directory(dir, ec) || ec) continue;

        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec) || ec) continue;

            auto path = entry.path();
            std::string baseName = path.stem().string();

            ToolMetadata meta;
            meta.toolId = baseName;
            meta.schema.inputSchemaHash = fnv1a(baseName);
            meta.schema.outputSchemaHash = fnv1a(baseName) ^ kFnvPrime;
            meta.reliability = kBaseReliability;
            meta.cost = 1;
            meta.riskLevel = ToolRiskLevel::LOW;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanPackageManagers() {
    // Detect installed package managers by checking known binary locations
    struct PkgManager {
        const char* binary;
        const char* toolId;
        ToolRiskLevel risk;
    };

    // Binary names only — no hardcoded English descriptions
    static constexpr size_t kPkgManagerCount = 8;
    const PkgManager managers[kPkgManagerCount] = {
        {"npm",    "npm",    ToolRiskLevel::LOW},
        {"yarn",   "yarn",   ToolRiskLevel::LOW},
        {"pip",    "pip",    ToolRiskLevel::LOW},
        {"pip3",   "pip3",   ToolRiskLevel::LOW},
        {"cargo",  "cargo",  ToolRiskLevel::LOW},
        {"brew",   "brew",   ToolRiskLevel::LOW},
        {"apt",    "apt",    ToolRiskLevel::MEDIUM},
        {"choco",  "choco",  ToolRiskLevel::MEDIUM}
    };

    for (size_t i = 0; i < kPkgManagerCount; ++i) {
        const auto& mgr = managers[i];
        // Check if binary is accessible in PATH
        std::string binaryName = mgr.binary;
#ifdef _WIN32
        binaryName += ".exe";
#endif
        const char* pathEnv = std::getenv("PATH");
        if (!pathEnv) continue;

        std::string pathStr(pathEnv);
#ifdef _WIN32
        constexpr char kSep = ';';
#else
        constexpr char kSep = ':';
#endif
        std::string dir;
        bool found = false;
        for (char c : pathStr) {
            if (c == kSep) {
                std::error_code ec;
                auto fullPath = std::filesystem::path(dir) / binaryName;
                if (std::filesystem::exists(fullPath, ec) && !ec) {
                    found = true;
                    break;
                }
                dir.clear();
            } else {
                dir += c;
            }
        }
        if (!dir.empty() && !found) {
            std::error_code ec;
            auto fullPath = std::filesystem::path(dir) / binaryName;
            if (std::filesystem::exists(fullPath, ec) && !ec) {
                found = true;
            }
        }

        if (found) {
            ToolMetadata meta;
            meta.toolId = mgr.toolId;
            meta.schema.inputSchemaHash = fnv1a(mgr.toolId);
            meta.schema.outputSchemaHash = fnv1a(mgr.toolId) ^ kFnvPrime;
            meta.reliability = kBaseReliability;
            meta.cost = 2;
            meta.riskLevel = mgr.risk;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanKnownIDEs() {
    // Check filesystem paths and registry for IDE installations
    struct IdeEntry {
        const char* toolId;
#ifdef _WIN32
        const char* winPath;
#else
        const char* unixPath;
#endif
    };

#ifdef _WIN32
    const std::vector<std::pair<std::string, std::string>> idePaths = {
        {"vscode", "C:\\Program Files\\Microsoft VS Code\\Code.exe"},
        {"vscode_user", std::string(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : "") + "\\Programs\\Microsoft VS Code\\Code.exe"},
        {"android_studio", "C:\\Program Files\\Android\\Android Studio\\bin\\studio64.exe"},
        {"clion", "C:\\Program Files\\JetBrains\\CLion\\bin\\clion64.exe"},
        {"idea", "C:\\Program Files\\JetBrains\\IntelliJ IDEA\\bin\\idea64.exe"},
        {"visual_studio", "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\devenv.exe"}
    };
#else
    const std::vector<std::pair<std::string, std::string>> idePaths = {
        {"vscode", "/usr/bin/code"},
        {"android_studio", "/opt/android-studio/bin/studio.sh"},
        {"clion", "/opt/clion/bin/clion.sh"},
        {"idea", "/opt/idea/bin/idea.sh"}
    };
#endif

    for (const auto& [toolId, path] : idePaths) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && !ec) {
            ToolMetadata meta;
            meta.toolId = toolId;
            meta.schema.inputSchemaHash = fnv1a(toolId);
            meta.schema.outputSchemaHash = fnv1a(toolId) ^ kFnvPrime;
            meta.reliability = kHighReliability;
            meta.cost = 3;
            meta.riskLevel = ToolRiskLevel::NONE;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanCloudServices() {
    // Detect cloud CLI tools by checking PATH
    struct CloudCLI {
        const char* binary;
        const char* toolId;
    };

    static constexpr size_t kCloudCLICount = 6;
    const CloudCLI clis[kCloudCLICount] = {
        {"aws",       "aws_cli"},
        {"gcloud",    "gcloud_cli"},
        {"az",        "azure_cli"},
        {"doctl",     "digitalocean_cli"},
        {"heroku",    "heroku_cli"},
        {"firebase",  "firebase_cli"}
    };

    for (size_t i = 0; i < kCloudCLICount; ++i) {
        const auto& cli = clis[i];
        std::string binaryName = cli.binary;
#ifdef _WIN32
        binaryName += ".exe";
#endif
        const char* pathEnv = std::getenv("PATH");
        if (!pathEnv) continue;

        std::string pathStr(pathEnv);
#ifdef _WIN32
        constexpr char kSep = ';';
#else
        constexpr char kSep = ':';
#endif
        std::string dir;
        bool found = false;
        for (char c : pathStr) {
            if (c == kSep) {
                std::error_code ec;
                auto fullPath = std::filesystem::path(dir) / binaryName;
                if (std::filesystem::exists(fullPath, ec) && !ec) { found = true; break; }
                dir.clear();
            } else {
                dir += c;
            }
        }
        if (!dir.empty() && !found) {
            std::error_code ec;
            auto fullPath = std::filesystem::path(dir) / binaryName;
            if (std::filesystem::exists(fullPath, ec) && !ec) { found = true; }
        }

        if (found) {
            ToolMetadata meta;
            meta.toolId = cli.toolId;
            meta.schema.inputSchemaHash = fnv1a(cli.toolId);
            meta.schema.outputSchemaHash = fnv1a(cli.toolId) ^ kFnvPrime;
            meta.reliability = kBaseReliability;
            meta.cost = 5;
            meta.riskLevel = ToolRiskLevel::MEDIUM;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanLocalServices() {
    // Check for common local development services via filesystem markers
    struct ServiceMarker {
        const char* toolId;
        const char* markerFile;
    };

    // Check for docker, kubernetes, database servers by their config/binary presence
#ifdef _WIN32
    const std::vector<std::pair<std::string, std::string>> serviceMarkers = {
        {"docker", "C:\\Program Files\\Docker\\Docker\\Docker Desktop.exe"},
        {"kubectl", "C:\\Program Files\\Docker\\Docker\\resources\\bin\\kubectl.exe"},
        {"mysql", "C:\\Program Files\\MySQL\\MySQL Server 8.0\\bin\\mysql.exe"},
        {"postgres", "C:\\Program Files\\PostgreSQL\\16\\bin\\psql.exe"},
        {"redis", "C:\\Program Files\\Redis\\redis-server.exe"}
    };
#else
    const std::vector<std::pair<std::string, std::string>> serviceMarkers = {
        {"docker", "/usr/bin/docker"},
        {"kubectl", "/usr/bin/kubectl"},
        {"mysql", "/usr/bin/mysql"},
        {"postgres", "/usr/bin/psql"},
        {"redis", "/usr/bin/redis-server"}
    };
#endif

    for (const auto& [toolId, path] : serviceMarkers) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && !ec) {
            ToolMetadata meta;
            meta.toolId = toolId;
            meta.schema.inputSchemaHash = fnv1a(toolId);
            meta.schema.outputSchemaHash = fnv1a(toolId) ^ kFnvPrime;
            meta.reliability = kBaseReliability;
            meta.cost = 2;
            meta.riskLevel = ToolRiskLevel::MEDIUM;
            discovered_.push_back(meta);
        }
    }
}

// ---- Platform App Discovery ----

void ToolDiscovery::scanMacOSApplications() {
    const std::vector<std::pair<std::string, std::string>> macApps = {
        {"com.apple.Music", "Music"},
        {"com.apple.Safari", "Safari"},
        {"com.apple.dt.Xcode", "Xcode"},
        {"com.apple.Terminal", "Terminal"},
        {"com.apple.finder", "Finder"},
        {"com.apple.mail", "Mail"},
        {"com.apple.Notes", "Notes"}
    };

    for (const auto& [bundleId, name] : macApps) {
        if (isMacOSAppInstalled(bundleId)) {
            ToolMetadata meta;
            meta.toolId = "macos_" + name;
            meta.schema.inputSchemaHash = fnv1a(bundleId);
            meta.schema.outputSchemaHash = fnv1a(bundleId) ^ kFnvPrime;
            meta.reliability = kHighReliability;
            meta.cost = 1;
            meta.riskLevel = ToolRiskLevel::NONE;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanWindowsProgramFiles() {
    const std::vector<std::string> winApps = {
        "VSCode", "Chrome", "Firefox", "Excel", "Word", "PowerShell",
        "Notepad++", "7-Zip", "WinRAR", "Git", "Python"
    };

    for (const auto& appName : winApps) {
        if (isWindowsAppInstalled(appName)) {
            ToolMetadata meta;
            meta.toolId = "win_" + appName;
            meta.schema.inputSchemaHash = fnv1a(appName);
            meta.schema.outputSchemaHash = fnv1a(appName) ^ kFnvPrime;
            meta.reliability = kHighReliability;
            meta.cost = 1;
            meta.riskLevel = ToolRiskLevel::NONE;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanLinuxDesktopEntries() {
    const std::vector<std::string> linuxApps = {
        "firefox.desktop", "gimp.desktop", "vlc.desktop",
        "org.gnome.Terminal.desktop", "org.gnome.Nautilus.desktop",
        "libreoffice-calc.desktop", "code.desktop"
    };

    for (const auto& desktopFile : linuxApps) {
        if (isLinuxAppInstalled(desktopFile)) {
            std::string name = desktopFile.substr(0, desktopFile.find('.'));
            ToolMetadata meta;
            meta.toolId = "linux_" + name;
            meta.schema.inputSchemaHash = fnv1a(desktopFile);
            meta.schema.outputSchemaHash = fnv1a(desktopFile) ^ kFnvPrime;
            meta.reliability = kHighReliability;
            meta.cost = 1;
            meta.riskLevel = ToolRiskLevel::NONE;
            discovered_.push_back(meta);
        }
    }
}

void ToolDiscovery::scanMobileURLSchemes() {
    // URL scheme detection is platform-specific (iOS: canOpenURL)
    // On desktop, we register known schemes as potential remote tools
    const std::vector<std::string> schemes = {
        "music://", "whatsapp://", "slack://", "maps://", "tel://", "mailto://"
    };

    for (const auto& scheme : schemes) {
        std::string name = scheme.substr(0, scheme.find(':'));
        ToolMetadata meta;
        meta.toolId = "scheme_" + name;
        meta.schema.inputSchemaHash = fnv1a(scheme);
        meta.schema.outputSchemaHash = fnv1a(scheme) ^ kFnvPrime;
        meta.reliability = 0.70f;
        meta.cost = 1;
        meta.riskLevel = ToolRiskLevel::LOW;
        discovered_.push_back(meta);
    }
}

// ---- Platform detection helpers ----

bool ToolDiscovery::isMacOSAppInstalled(const std::string& bundleId) {
    std::string appName = bundleId;
    size_t lastDot = bundleId.find_last_of('.');
    if (lastDot != std::string::npos) {
        appName = bundleId.substr(lastDot + 1);
    }

    std::vector<std::string> searchPaths = {
        "/Applications/" + appName + ".app",
        "/Applications/Utilities/" + appName + ".app",
        "/System/Applications/" + appName + ".app",
        "/System/Applications/Utilities/" + appName + ".app"
    };

    for (const auto& path : searchPaths) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && !ec) {
            return true;
        }
    }
    return false;
}

bool ToolDiscovery::isWindowsAppInstalled(const std::string& appName) {
#ifdef _WIN32
    // Check Windows registry for installed programs
    HKEY hKey;
    const char* subkey = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        DWORD numSubKeys = 0;
        RegQueryInfoKey(hKey, nullptr, nullptr, nullptr, &numSubKeys,
                        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

        for (DWORD i = 0; i < numSubKeys; ++i) {
            char subKeyName[256] = {};
            DWORD subKeyLen = sizeof(subKeyName);
            if (RegEnumKeyExA(hKey, i, subKeyName, &subKeyLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                HKEY hSubKey;
                if (RegOpenKeyExA(hKey, subKeyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                    char displayName[256] = {};
                    DWORD displayNameLen = sizeof(displayName);
                    DWORD type = 0;
                    if (RegQueryValueExA(hSubKey, "DisplayName", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(displayName), &displayNameLen) == ERROR_SUCCESS) {
                        std::string name(displayName);
                        // Case-insensitive substring search using hash comparison
                        // Transform both to lowercase for comparison
                        std::string lowerName = name;
                        std::string lowerApp = appName;
                        for (auto& c : lowerName) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
                        for (auto& c : lowerApp) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
                        if (lowerName.find(lowerApp) != std::string::npos) {
                            RegCloseKey(hSubKey);
                            RegCloseKey(hKey);
                            return true;
                        }
                    }
                    RegCloseKey(hSubKey);
                }
            }
        }
        RegCloseKey(hKey);
    }

    // Fallback: check Program Files directories
    std::vector<std::string> searchPaths = {
        "C:\\Program Files\\" + appName,
        "C:\\Program Files (x86)\\" + appName
    };
    for (const auto& p : searchPaths) {
        std::error_code ec;
        if (std::filesystem::exists(p, ec) && !ec) {
            return true;
        }
    }
#else
    (void)appName;
#endif
    return false;
}

bool ToolDiscovery::isLinuxAppInstalled(const std::string& desktopFile) {
    std::vector<std::string> searchPaths = {
        "/usr/share/applications/" + desktopFile,
        "/usr/local/share/applications/" + desktopFile,
        "/var/lib/flatpak/exports/share/applications/" + desktopFile
    };

    const char* home = std::getenv("HOME");
    if (home) {
        searchPaths.push_back(std::string(home) + "/.local/share/applications/" + desktopFile);
    }

    for (const auto& p : searchPaths) {
        std::error_code ec;
        if (std::filesystem::exists(p, ec) && !ec) {
            return true;
        }
    }
    return false;
}

void ToolDiscovery::registerDiscoveredTools(ToolRegistry* registry) {
    if (!registry) return;
    registry->registerDiscovered(discovered_);
}

void ToolDiscovery::registerAll(ToolRegistry* registry) {
    scanPathEnvironment();
    scanPluginDirectories();
    scanPackageManagers();
    scanKnownIDEs();
    scanCloudServices();
    scanLocalServices();
    scanMacOSApplications();
    scanWindowsProgramFiles();
    scanLinuxDesktopEntries();
    scanMobileURLSchemes();
    registerDiscoveredTools(registry);
}

ToolMetadata ToolDiscovery::inferFromPath(const std::string& path) {
    ToolMetadata meta;
    std::filesystem::path p(path);
    meta.toolId = p.stem().string();
    meta.schema.inputSchemaHash = fnv1a(meta.toolId);
    meta.schema.outputSchemaHash = fnv1a(meta.toolId) ^ kFnvPrime;
    meta.reliability = kUnknownReliability;
    meta.cost = 1;
    meta.riskLevel = ToolRiskLevel::LOW;
    return meta;
}

bool ToolDiscovery::isKnownExecutable(const std::string& name) {
    // Hash-based lookup against known high-reliability tool hashes
    static const uint64_t knownHashes[] = {
        fnv1a("git"), fnv1a("docker"), fnv1a("python"), fnv1a("python3"),
        fnv1a("node"), fnv1a("npm"), fnv1a("cargo"), fnv1a("cmake"),
        fnv1a("make"), fnv1a("gcc"), fnv1a("clang"), fnv1a("java"),
        fnv1a("go"), fnv1a("rustc"), fnv1a("adb"), fnv1a("curl"),
        fnv1a("ssh"), fnv1a("scp"), fnv1a("tar"), fnv1a("zip")
    };

    uint64_t nameHash = fnv1a(name);
    for (uint64_t knownHash : knownHashes) {
        if (nameHash == knownHash) return true;
    }
    return false;
}

} // namespace research
} // namespace yuki
