#pragma once
// BackgroundAgents.h — Background task manager + browser agent (merged from BackgroundTaskManager + BrowserAgent)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <map>
#include <vector>

// ── §BackgroundTaskManager ────────────────────────────────────────────────────

enum class TaskState { QUEUED, RUNNING, COMPLETED, FAILED, CANCELLED };

struct BackgroundTask {
    std::string taskId;
    std::string description;
    TaskState   state    = TaskState::QUEUED;
    std::string result;
    int         progress = 0;
};

using TaskFn       = std::function<std::string()>;
using CompletionFn = std::function<void(const std::string& taskId,
                                        bool success,
                                        const std::string& result)>;

class BackgroundTaskManager {
public:
    BackgroundTaskManager();
    ~BackgroundTaskManager();
    void start();
    void stop();
    std::string submit(const std::string& description, TaskFn fn, CompletionFn onDone = nullptr);
    void        cancel(const std::string& taskId);
    TaskState   getState(const std::string& taskId) const;
    std::string getResult(const std::string& taskId) const;
    std::string describeAllTasks() const;
private:
    struct QueueItem { std::string taskId, description; TaskFn fn; CompletionFn onDone; };
    void workerLoop();
    std::vector<std::thread>          workers_;
    std::queue<QueueItem>             queue_;
    mutable std::mutex                mu_;
    std::condition_variable           cv_;
    std::atomic<bool>                 stopping_{false};
    mutable std::mutex                statusMu_;
    std::map<std::string, BackgroundTask> tasks_;
    int nextId_ = 1;
    static std::string makeId(int n);
};

// ── §BrowserAgent ─────────────────────────────────────────────────────────────

struct BrowserResult {
    bool        success = false;
    std::string detail;
};

class BrowserAgent {
public:
    BrowserAgent();
    ~BrowserAgent();
    bool start();
    void stop();
    bool isRunning() const { return running_; }
    BrowserResult sendWhatsApp(const std::string& contact, const std::string& message, int timeoutMs = 15000);
    BrowserResult openUrl(const std::string& url, int timeoutMs = 5000);
    BrowserResult findTab(const std::string& keyword, int timeoutMs = 5000);
private:
    BrowserResult sendAndWait(const std::string& jsonCmd, int timeoutMs);
    bool          sendLine(const std::string& json);
    void          readLoop();
    HANDLE hProc_   = INVALID_HANDLE_VALUE;
    HANDLE hThread_ = INVALID_HANDLE_VALUE;
    HANDLE hWrite_  = INVALID_HANDLE_VALUE;
    HANDLE hRead_   = INVALID_HANDLE_VALUE;
    std::atomic<bool>  running_{false};
    std::atomic<bool>  ready_  {false};
    std::thread        readThread_;
    mutable std::mutex          resultMu_;
    std::condition_variable     resultCv_;
    BrowserResult               lastResult_;
    std::atomic<bool>           resultReady_{false};
    static std::string jStr(const std::string& s);
};
