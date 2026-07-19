#pragma once
#include "ExecutionTypes.h"
#include <string>
#include <filesystem>
#include <vector>

class FileOperator {
public:
    FileOperator();
    StepResult execute(const ActionStep& step);
    bool undo(const UndoRecord& record);

private:
    StepResult safeDeleteToReview(const std::string& path);
    StepResult createFile(const std::string& path, const std::string& content);
    StepResult copyItem(const std::string& source, const std::string& dest);
    StepResult moveItem(const std::string& source, const std::string& dest);
    StepResult renameItem(const std::string& path, const std::string& newName);

    void logUndo(const UndoRecord& record);
    StepResult makeSuccess(const std::string& msg);
    StepResult makeFailure(const std::string& msg);

    std::filesystem::path reviewRoot_;
    std::vector<UndoRecord> undoLog_;
};
