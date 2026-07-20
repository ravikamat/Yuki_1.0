#include "FileOperator.h"
#include <fstream>
#include <chrono>
#include <iostream>

namespace {
    // =============================================================================
    // ATOMIC FILE WRITE — Write-to-temp-then-rename pattern
    // =============================================================================
    // Writes content to a temporary file in the same directory as the target,
    // flushes the stream, then renames the temp file to the target name.
    // On Windows NTFS and POSIX, rename() on the same volume is atomic.
    // Prevents partial/corrupted files if the process crashes during writing.
    // =============================================================================
    bool atomicWriteFile(const std::filesystem::path& targetPath,
                         const std::string& content,
                         std::string& outError) {
        namespace fs = std::filesystem;
        try {
            fs::path dir = targetPath.parent_path();
            if (dir.empty()) dir = fs::current_path();

            // Generate a unique temp filename using high-resolution timestamp
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            fs::path tempPath = dir / (targetPath.stem().string() + ".tmp." +
                                       std::to_string(now) +
                                       targetPath.extension().string());

            // Step 1: Write to temp file (binary mode to avoid CRLF surprises)
            {
                std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
                if (!ofs.is_open()) {
                    outError = "Failed to open temp file: " + tempPath.string();
                    return false;
                }
                ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
                if (!ofs.good()) {
                    outError = "Failed to write temp file: " + tempPath.string();
                    fs::remove(tempPath); // Best-effort cleanup
                    return false;
                }
                ofs.flush(); // Ensure OS buffer is flushed before rename
            }

            // Step 2: Atomic rename (temp → target)
            // Windows std::filesystem::rename does not overwrite existing files,
            // so remove the target first if it exists.
            if (fs::exists(targetPath)) {
                fs::remove(targetPath);
            }
            fs::rename(tempPath, targetPath);
            return true;

        } catch (const std::filesystem::filesystem_error& e) {
            outError = std::string("Filesystem error: ") + e.what();
            return false;
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
            return false;
        }
    }
} // anonymous namespace

FileOperator::FileOperator() {
    reviewRoot_ = std::filesystem::current_path() / "data" / "review";
    std::filesystem::create_directories(reviewRoot_);
}

StepResult FileOperator::execute(const ActionStep& step) {
    auto pathIt = step.args.find("path");
    std::string path = pathIt != step.args.end() ? pathIt->second : "";

    if (step.commandOrApi == "create") {
        auto contentIt = step.args.find("content");
        return createFile(path, contentIt != step.args.end() ? contentIt->second : "");
    }
    if (step.commandOrApi == "delete") {
        return safeDeleteToReview(path);
    }
    if (step.commandOrApi == "copy" || step.commandOrApi == "move") {
        auto destIt = step.args.find("dest");
        std::string dest = destIt != step.args.end() ? destIt->second : "";
        if (step.commandOrApi == "copy") return copyItem(path, dest);
        return moveItem(path, dest);
    }
    if (step.commandOrApi == "rename") {
        auto nameIt = step.args.find("newName");
        return renameItem(path, nameIt != step.args.end() ? nameIt->second : "");
    }
    return makeFailure("Unsupported FileOperator command: " + step.commandOrApi);
}

StepResult FileOperator::safeDeleteToReview(const std::string& path) {
    if (!std::filesystem::exists(path)) return makeFailure("Path does not exist: " + path);
    std::filesystem::path review = reviewRoot_ / std::filesystem::path(path).filename();
    
    // Add timestamp to prevent overwriting in review folder
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    review += "_" + std::to_string(now);

    std::error_code ec;
    std::filesystem::rename(path, review, ec);
    if (ec) {
        // Fallback to copy+delete if rename fails across drives
        std::filesystem::copy(path, review, std::filesystem::copy_options::recursive, ec);
        if (!ec) std::filesystem::remove_all(path, ec);
    }

    if (ec) return makeFailure("Failed to safe delete: " + ec.message());

    logUndo({"op_" + std::to_string(now), "MOVE_TO_REVIEW", path, review.string(), now, true});
    return makeSuccess("Moved to review folder instead of permanent delete");
}

StepResult FileOperator::createFile(const std::string& path, const std::string& content) {
    namespace fs = std::filesystem;
    try {
        // Resolve to absolute/canonical path (does not require file to exist)
        fs::path targetPath = fs::weakly_canonical(path);

        // Ensure parent directory exists
        fs::path parentDir = targetPath.parent_path();
        if (!parentDir.empty() && !fs::exists(parentDir)) {
            fs::create_directories(parentDir);
        }

        // Atomic write via temp-then-rename
        std::string error;
        if (!atomicWriteFile(targetPath, content, error)) {
            return makeFailure("Atomic write failed: " + error);
        }

        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        logUndo({"op_" + std::to_string(now), "CREATE", "", targetPath.string(), now, true});
        return makeSuccess("File created (atomic).");
    } catch (const std::exception& e) {
        return makeFailure(std::string("Exception during create: ") + e.what());
    }
}


StepResult FileOperator::copyItem(const std::string& source, const std::string& dest) {
    std::error_code ec;
    std::filesystem::copy(source, dest, std::filesystem::copy_options::recursive, ec);
    if (ec) return makeFailure("Failed to copy: " + ec.message());
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    logUndo({"op_" + std::to_string(now), "COPY", source, dest, now, true});
    return makeSuccess("Copied successfully.");
}

StepResult FileOperator::moveItem(const std::string& source, const std::string& dest) {
    std::error_code ec;
    std::filesystem::rename(source, dest, ec);
    if (ec) return makeFailure("Failed to move: " + ec.message());
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    logUndo({"op_" + std::to_string(now), "MOVE", source, dest, now, true});
    return makeSuccess("Moved successfully.");
}

StepResult FileOperator::renameItem(const std::string& path, const std::string& newName) {
    std::error_code ec;
    std::filesystem::path dest = std::filesystem::path(path).parent_path() / newName;
    std::filesystem::rename(path, dest, ec);
    if (ec) return makeFailure("Failed to rename: " + ec.message());
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    logUndo({"op_" + std::to_string(now), "RENAME", path, dest.string(), now, true});
    return makeSuccess("Renamed successfully.");
}

bool FileOperator::undo(const UndoRecord& record) {
    std::error_code ec;
    if (record.action == "MOVE_TO_REVIEW" || record.action == "MOVE" || record.action == "RENAME") {
        std::filesystem::rename(record.targetPath, record.sourcePath, ec);
        return !ec;
    }
    if (record.action == "CREATE" || record.action == "COPY") {
        std::filesystem::remove_all(record.targetPath, ec);
        return !ec;
    }
    return false;
}

void FileOperator::logUndo(const UndoRecord& record) {
    undoLog_.push_back(record);
}

StepResult FileOperator::makeSuccess(const std::string& msg) {
    StepResult sr; sr.success = true; sr.summary = msg; return sr;
}

StepResult FileOperator::makeFailure(const std::string& msg) {
    StepResult sr; sr.success = false; sr.summary = msg; return sr;
}
