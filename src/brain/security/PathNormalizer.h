#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace yuki {
namespace security {

class PathNormalizer {
public:
    struct NormalizedPath {
        std::filesystem::path absolute;
        bool is_valid = false;
        std::string rejection_reason;
    };

    // Resolves . and .. components WITHOUT requiring path existence.
    // Handles: relative paths, absolute paths, mixed separators, redundant slashes.
    // Does NOT resolve symlinks (symlinks are validated separately via final exists+read_symlink check).
    static NormalizedPath normalize(
        const std::filesystem::path& input,
        const std::filesystem::path& base_directory = std::filesystem::current_path()
    );

private:
    static std::vector<std::string> splitComponents(const std::filesystem::path& p);
    static bool isDevicePath(const std::string& s);
    static bool containsNullByte(const std::string& s);
};

} // namespace security
} // namespace yuki
