#include "brain/research/tools/SandboxExecuteTool.h"
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <windows.h>

namespace yuki {
namespace research {

ToolResult SandboxExecuteTool::execute(const std::vector<uint8_t>& input) {
    ToolResult result;
    result.status = ToolStatus::SUCCESS;
    result.confidence = 0.95f;
    
    std::string command(input.begin(), input.end());
    if (command.empty()) {
        result.status = ToolStatus::INVALID_INPUT;
        result.confidence = 0.0f;
        std::string err = "Empty command";
        result.payload = std::vector<uint8_t>(err.begin(), err.end());
        return result;
    }

    // Create a temporary directory for sandbox
    std::filesystem::path sandbox_dir = std::filesystem::temp_directory_path() / "yuki_sandbox";
    std::filesystem::create_directories(sandbox_dir);
    
    // Write command to a batch file
    std::filesystem::path script_path = sandbox_dir / "sandbox_cmd.bat";
    std::ofstream script(script_path);
    script << "@echo off\n";
    script << command << "\n";
    script.close();

    std::stringstream ss;
    ss << "Sandbox execution:\n";
    ss << "  Command: " << command << "\n";
    ss << "  Working dir: " << sandbox_dir.string() << "\n";

    // Execute with timeout
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    
    std::string cmd = "cmd.exe /c " + script_path.string();
    PROCESS_INFORMATION pi{};
    
    BOOL ok = CreateProcessA(nullptr,
                              const_cast<char*>(cmd.c_str()),
                              nullptr, nullptr,
                              TRUE, CREATE_NO_WINDOW,
                              nullptr, sandbox_dir.string().c_str(),
                              &si, &pi);
    CloseHandle(hWritePipe);
    
    if (!ok) {
        result.status = ToolStatus::UNKNOWN_ERROR;
        result.confidence = 0.0f;
        std::string err = "Failed to create process";
        result.payload = std::vector<uint8_t>(err.begin(), err.end());
        return result;
    }
    
    // Wait with timeout (30 seconds)
    DWORD waitRes = WaitForSingleObject(pi.hProcess, 30000);
    
    // Read output
    std::string output;
    char buf[1024];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf)-1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = 0;
        output += buf;
    }
    CloseHandle(hReadPipe);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (waitRes == WAIT_TIMEOUT) {
        result.status = ToolStatus::TIMEOUT;
        result.confidence = 0.0f;
        output += "\n[TIMEOUT after 30s]";
    } else if (exitCode != 0) {
        result.status = ToolStatus::UNKNOWN_ERROR;
        result.confidence = 0.5f;
    }
    
    ss << "  Exit code: " << exitCode << "\n";
    ss << "  Output:\n";
    if (output.length() > 2000) {
        ss << output.substr(0, 2000) << "\n  ... [truncated]";
    } else {
        ss << output;
    }

    std::string out = ss.str();
    result.payload = std::vector<uint8_t>(out.begin(), out.end());
    return result;
}

ToolMetadata SandboxExecuteTool::getMetadata() const {
    ToolMetadata meta;
    meta.toolId = "sandbox_execute";
    meta.schema.inputSchemaHash = 0x1239;
    meta.schema.outputSchemaHash = 0x567D;
    meta.reliability = 0.95f;
    meta.cost = 3;
    meta.riskLevel = ToolRiskLevel::LOW;
    return meta;
}

} // namespace research
} // namespace yuki
