#include "brain/security/PathNormalizer.h"
#include <algorithm>
#include <cctype>

using namespace yuki::security;

std::vector<std::string> PathNormalizer::splitComponents(const std::filesystem::path& p) {
    std::vector<std::string> components;
    for (const auto& part : p) {
        std::string s = part.string();
        if (containsNullByte(s)) {
            return {}; // signal invalid null byte injection
        }
        components.push_back(std::move(s));
    }
    return components;
}

bool PathNormalizer::isDevicePath(const std::string& s) {
    // Windows device paths: \\.\, \\?\, CON, PRN, AUX, NUL, COM1-9, LPT1-9
    if (s.size() >= 4 && s.substr(0, 4) == "\\\\.\\") return true;
    if (s.size() >= 4 && s.substr(0, 4) == "\\\\?\\") return true;
    if (s.size() >= 4 && s.substr(0, 4) == "//./") return true;
    if (s.size() >= 4 && s.substr(0, 4) == "//?/") return true;
    
    std::string upper = s;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(::toupper(c)); });
    
    static const std::vector<std::string> devices = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    for (const auto& dev : devices) {
        if (upper == dev) return true;
    }
    return false;
}

bool PathNormalizer::containsNullByte(const std::string& s) {
    return s.find('\0') != std::string::npos;
}

PathNormalizer::NormalizedPath PathNormalizer::normalize(
    const std::filesystem::path& input,
    const std::filesystem::path& base_directory
) {
    NormalizedPath result;
    result.is_valid = false;

    std::string raw = input.string();
    if (raw.empty()) {
        result.rejection_reason = "EMPTY_PATH";
        return result;
    }

    if (containsNullByte(raw)) {
        result.rejection_reason = "NULL_BYTE_INJECTION";
        return result;
    }

    // 1. Determine if absolute
    bool is_absolute = input.is_absolute();
    std::filesystem::path working_base = is_absolute ? std::filesystem::path() : base_directory;

    // 2. Split into components
    std::vector<std::string> components = splitComponents(input);
    if (components.empty() && !raw.empty()) {
        result.rejection_reason = "NULL_BYTE_INJECTION";
        return result;
    }

    // 3. Check for device path injection
    if (!components.empty() && isDevicePath(components[0])) {
        result.rejection_reason = "DEVICE_PATH_INJECTION";
        return result;
    }
    if (isDevicePath(raw)) {
        result.rejection_reason = "DEVICE_PATH_INJECTION";
        return result;
    }

    // 4. Build stack, resolving . and ..
    std::vector<std::string> stack;
    for (const auto& comp : components) {
        if (comp == "." || comp == "/" || comp == "\\" || comp.empty()) {
            continue;
        } else if (comp == "..") {
            if (!stack.empty()) {
                if (stack.back() == "..") {
                    if (!is_absolute) {
                        stack.push_back("..");
                    } else {
                        result.rejection_reason = "PATH_TRAVERSAL_ABOVE_ROOT";
                        return result;
                    }
                } else {
                    stack.pop_back();
                }
            } else if (!is_absolute) {
                // Relative path going above base: track overflow
                stack.push_back("..");
            } else {
                // Absolute path going above root
                result.rejection_reason = "PATH_TRAVERSAL_ABOVE_ROOT";
                return result;
            }
        } else {
            stack.push_back(comp);
        }
    }

    // Check if relative path overflows base
    if (!is_absolute) {
        for (const auto& s : stack) {
            if (s == "..") {
                result.rejection_reason = "PATH_TRAVERSAL_ESCAPE_BASE";
                return result;
            }
        }
    }

    // 5. Reconstruct path
    std::filesystem::path reconstructed;
    if (is_absolute) {
        if (input.has_root_name()) {
            reconstructed = input.root_name();
        }
        reconstructed /= input.root_directory();
        for (const auto& s : stack) {
            reconstructed /= s;
        }
    } else {
        reconstructed = working_base;
        for (const auto& s : stack) {
            reconstructed /= s;
        }
    }

    // 6. Final logical canonicalization: resolve base + relative combination
    std::vector<std::string> final_comps;
    for (const auto& part : reconstructed) {
        std::string s = part.string();
        if (s == "." || s == "/" || s == "\\" || s.empty()) continue;
        if (s == "..") {
            if (!final_comps.empty()) {
                final_comps.pop_back();
            } else {
                result.rejection_reason = "PATH_TRAVERSAL_ESCAPE_BASE";
                return result;
            }
        } else {
            final_comps.push_back(std::move(s));
        }
    }

    // 7. Build final absolute path
    std::filesystem::path final_path;
    if (reconstructed.has_root_name()) {
        final_path = reconstructed.root_name();
    } else if (base_directory.has_root_name() && !is_absolute) {
        final_path = base_directory.root_name();
    }
    
    if (reconstructed.has_root_directory() || base_directory.has_root_directory()) {
        final_path /= std::filesystem::path("/").root_directory();
    }
    
    for (const auto& s : final_comps) {
        // Skip root_name part if it was already added to final_path
        if (reconstructed.has_root_name() && s == reconstructed.root_name().string()) continue;
        if (base_directory.has_root_name() && s == base_directory.root_name().string()) continue;
        final_path /= s;
    }

    result.absolute = final_path;
    result.is_valid = true;
    return result;
}
