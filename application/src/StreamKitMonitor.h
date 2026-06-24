#pragma once

#include "DiscordModels.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class StreamKitMonitor final {
public:
    using StatusHandler = std::function<void(const std::string&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using LogHandler = std::function<void(const std::string&)>;
    using UsersHandler = std::function<void(const std::vector<DiscordVoiceUser>&)>;

    struct BrowserCandidate {
        std::string name;
        std::string path;
    };

    StreamKitMonitor();
    ~StreamKitMonitor();

    StreamKitMonitor(const StreamKitMonitor&) = delete;
    StreamKitMonitor& operator=(const StreamKitMonitor&) = delete;

    static std::vector<BrowserCandidate> DetectBrowsers();

    bool IsRunning() const;
    void SetStatusHandler(StatusHandler handler);
    void SetErrorHandler(ErrorHandler handler);
    void SetLogHandler(LogHandler handler);
    void SetUsersHandler(UsersHandler handler);
    bool Start(std::string browserPath, std::string overlayUrl, bool showBrowserWindow, bool bypassLocalNetworkPrompt, int pollIntervalMs);
    void Stop();

private:
    void WorkerLoop(std::string browserPath, std::string overlayUrl, bool showBrowserWindow, bool bypassLocalNetworkPrompt, int pollIntervalMs);
    bool LaunchBrowser(const std::string& browserPath, const std::string& overlayUrl, bool showBrowserWindow, bool bypassLocalNetworkPrompt);
    bool ConnectDevTools(const std::string& overlayUrl);
    bool SendEvaluateCommand();
    void ReceiveLoopOnce();
    void HandleDevToolsMessage(const std::string& message);
    void ReportStatus(const std::string& status);
    void ReportError(const std::string& error);
    void ReportLog(const std::string& message);
    void CloseWebSocket();
    void StopBrowser();

    static std::string GetDevToolsList(int port);
    static std::string ExtractWebSocketDebuggerUrl(const std::string& jsonText, const std::string& overlayUrl, std::string& selectedPageUrl);
    static bool ParseWebSocketUrl(const std::string& url, std::string& host, int& port, std::string& path);

    std::atomic_bool running_ = false;
    std::thread worker_;
    StatusHandler onStatus_;
    ErrorHandler onError_;
    LogHandler onLog_;
    UsersHandler onUsers_;
    int remoteDebuggingPort_ = 0;
    unsigned int nextCommandId_ = 1;
    int lastReportedUserCount_ = -1;

#ifdef _WIN32
    void* browserProcess_ = nullptr;
    void* browserThread_ = nullptr;
    void* webSocket_ = nullptr;
#endif
};
