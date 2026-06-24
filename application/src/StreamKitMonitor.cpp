#include "StreamKitMonitor.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string_view>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace {
constexpr int PollIntervalMs = 750;

const char* OverlayScrapeScript = R"js(
(() => {
  const clean = (value) => (value || '').replace(/\s+/g, ' ').trim();
  const classText = (element) => {
    if (!element) return '';
    const value = element.getAttribute('class');
    return typeof value === 'string' ? value : '';
  };
  const readRow = (row) => {
    const nameNode = row.querySelector('.voice_username .Voice_name__TALd9, .voice_username span, .name, [class*="name"], [class*="Name"], [data-name]');
    const avatarNode = row.querySelector('.voice_avatar, .avatar, [class*="avatar"], [class*="Avatar"]');
    const text = clean((nameNode || row).textContent);
    const classes = `${classText(row)} ${classText(nameNode)} ${classText(avatarNode)}`.toLowerCase();
    return {
      id: row.getAttribute('data-userid') || row.getAttribute('data-user-id') || row.getAttribute('data-id') || text,
      name: text,
      speaking: classes.includes('speaking') || classes.includes('wrapper_speaking') || !!row.querySelector('.speaking, [class*="speaking"], [class*="Speaking"]'),
      muted: classes.includes('self_mute') || classes.includes('mute') || classes.includes('muted'),
      deafened: classes.includes('self_deaf') || classes.includes('deaf') || classes.includes('deafened')
    };
  };

  let rows = [...document.querySelectorAll('.voice_state, .Voice_voiceState__OCoZh, [class*="voiceState"], [class*="VoiceState"]')];
  if (!rows.length) {
    rows = [...document.querySelectorAll('li')].filter((row) => clean(row.textContent));
  }

  return rows
    .map(readRow)
    .filter((user) => user.name)
    .filter((user, index, all) => all.findIndex((candidate) => candidate.id === user.id) === index);
})()
)js";

std::wstring Widen(const std::string& value)
{
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring output(length - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, output.data(), length);
    return output;
}

std::string Narrow(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string output(length - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, output.data(), length, nullptr, nullptr);
    return output;
}

std::string QuoteArg(const std::string& value)
{
    std::string quoted = "\"";
    for (const char ch : value) {
        quoted += ch == '"' ? "\\\"" : std::string(1, ch);
    }
    quoted += "\"";
    return quoted;
}

bool FileExists(const std::string& path)
{
    return std::filesystem::exists(std::filesystem::u8path(path));
}

size_t AppendCurlData(char* data, size_t size, size_t nmemb, void* userData)
{
    auto* output = static_cast<std::string*>(userData);
    output->append(data, size * nmemb);
    return size * nmemb;
}

std::string TruncateForLog(const std::string& value)
{
    constexpr size_t MaxLength = 1800;
    if (value.size() <= MaxLength) {
        return value;
    }
    return value.substr(0, MaxLength) + "...";
}

#ifdef _WIN32
int PickDebuggingPort()
{
    return 18000 + static_cast<int>(GetCurrentProcessId() % 20000);
}
#endif
}

StreamKitMonitor::StreamKitMonitor()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

StreamKitMonitor::~StreamKitMonitor()
{
    Stop();
    curl_global_cleanup();
}

std::vector<StreamKitMonitor::BrowserCandidate> StreamKitMonitor::DetectBrowsers()
{
    std::vector<BrowserCandidate> browsers;

#ifdef _WIN32
    wchar_t programFiles[MAX_PATH]{};
    wchar_t programFilesX86[MAX_PATH]{};
    wchar_t localAppData[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, SHGFP_TYPE_CURRENT, programFiles);
    SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, SHGFP_TYPE_CURRENT, programFilesX86);
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData);

    const std::vector<BrowserCandidate> candidates = {
        {"Microsoft Edge", Narrow(std::wstring(programFilesX86) + L"\\Microsoft\\Edge\\Application\\msedge.exe")},
        {"Microsoft Edge", Narrow(std::wstring(programFiles) + L"\\Microsoft\\Edge\\Application\\msedge.exe")},
        {"Google Chrome", Narrow(std::wstring(programFiles) + L"\\Google\\Chrome\\Application\\chrome.exe")},
        {"Google Chrome", Narrow(std::wstring(programFilesX86) + L"\\Google\\Chrome\\Application\\chrome.exe")},
        {"Google Chrome", Narrow(std::wstring(localAppData) + L"\\Google\\Chrome\\Application\\chrome.exe")},
        {"Brave", Narrow(std::wstring(programFiles) + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe")},
        {"Brave", Narrow(std::wstring(programFilesX86) + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe")}
    };

    for (const auto& candidate : candidates) {
        if (FileExists(candidate.path)) {
            browsers.push_back(candidate);
        }
    }
#endif

    return browsers;
}

bool StreamKitMonitor::IsRunning() const
{
    return running_;
}

void StreamKitMonitor::SetStatusHandler(StatusHandler handler) { onStatus_ = std::move(handler); }
void StreamKitMonitor::SetErrorHandler(ErrorHandler handler) { onError_ = std::move(handler); }
void StreamKitMonitor::SetLogHandler(LogHandler handler) { onLog_ = std::move(handler); }
void StreamKitMonitor::SetUsersHandler(UsersHandler handler) { onUsers_ = std::move(handler); }

bool StreamKitMonitor::Start(std::string browserPath, std::string overlayUrl, bool showBrowserWindow)
{
    if (running_) {
        ReportLog("Start requested while StreamKit monitor is already running.");
        return true;
    }
    if (browserPath.empty() || overlayUrl.empty()) {
        ReportError("Choose a browser and paste a StreamKit overlay URL first.");
        return false;
    }

    running_ = true;
    lastReportedUserCount_ = -1;
    worker_ = std::thread(&StreamKitMonitor::WorkerLoop, this, std::move(browserPath), std::move(overlayUrl), showBrowserWindow);
    return true;
}

void StreamKitMonitor::Stop()
{
    running_ = false;
    CloseWebSocket();
    StopBrowser();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void StreamKitMonitor::WorkerLoop(std::string browserPath, std::string overlayUrl, bool showBrowserWindow)
{
    if (!LaunchBrowser(browserPath, overlayUrl, showBrowserWindow)) {
        running_ = false;
        return;
    }

    ReportStatus(showBrowserWindow ? "Waiting for visible browser DevTools." : "Waiting for headless browser DevTools.");
    bool connected = false;
    for (int attempt = 0; running_ && attempt < 60; ++attempt) {
        if (ConnectDevTools(overlayUrl)) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    if (!connected) {
        ReportError("Could not connect to the browser DevTools endpoint.");
        running_ = false;
    }

    while (running_) {
        if (!SendEvaluateCommand()) {
            ReportError("Failed to send StreamKit poll command.");
            break;
        }
        ReceiveLoopOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(PollIntervalMs));
    }

    running_ = false;
    CloseWebSocket();
    StopBrowser();
}

bool StreamKitMonitor::LaunchBrowser(const std::string& browserPath, const std::string& overlayUrl, bool showBrowserWindow)
{
#ifdef _WIN32
    wchar_t tempPath[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempPath);
    remoteDebuggingPort_ = PickDebuggingPort();
    const auto profilePath = Narrow(std::wstring(tempPath) + L"ez-pngtuber-streamkit-profile-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(std::filesystem::u8path(profilePath));

    std::ostringstream command;
    command << QuoteArg(browserPath)
            << " --disable-gpu"
            << " --remote-debugging-address=127.0.0.1"
            << " --remote-debugging-port=" << remoteDebuggingPort_
            << " --user-data-dir=" << QuoteArg(profilePath)
            << " --no-first-run"
            << " --no-default-browser-check";
    if (!showBrowserWindow) {
        command << " --headless=new";
    } else {
        command << " --new-window";
    }
    command << " " << QuoteArg(overlayUrl);

    ReportLog("Launching browser: " + command.str());
    ReportLog("Browser profile: " + profilePath);
    ReportLog("DevTools URL: http://127.0.0.1:" + std::to_string(remoteDebuggingPort_) + "/json/list");

    auto applicationName = Widen(browserPath);
    auto commandLine = Widen(command.str());
    STARTUPINFOW startupInfo{};
    PROCESS_INFORMATION processInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = showBrowserWindow ? SW_SHOWNORMAL : SW_HIDE;

    const DWORD creationFlags = showBrowserWindow ? 0 : CREATE_NO_WINDOW;
    if (!CreateProcessW(applicationName.c_str(), commandLine.data(), nullptr, nullptr, FALSE, creationFlags, nullptr, nullptr, &startupInfo, &processInfo)) {
        ReportError("Could not launch the selected browser. Windows error " + std::to_string(GetLastError()) + ".");
        return false;
    }

    browserProcess_ = processInfo.hProcess;
    browserThread_ = processInfo.hThread;
    ReportLog("Browser process id: " + std::to_string(processInfo.dwProcessId));
    ReportStatus(showBrowserWindow ? "Browser window launched." : "Headless browser launched.");
    return true;
#else
    (void)browserPath;
    (void)overlayUrl;
    (void)showBrowserWindow;
    ReportError("StreamKit browser monitoring is currently implemented for Windows.");
    return false;
#endif
}

bool StreamKitMonitor::ConnectDevTools(const std::string& overlayUrl)
{
#ifdef _WIN32
    const auto list = GetDevToolsList(remoteDebuggingPort_);
    if (list.empty()) {
        return false;
    }

    ReportLog("DevTools targets: " + TruncateForLog(list));
    std::string selectedPageUrl;
    const auto webSocketUrl = ExtractWebSocketDebuggerUrl(list, overlayUrl, selectedPageUrl);
    ReportLog("Selected DevTools target: " + selectedPageUrl);
    ReportLog("Selected WebSocket: " + webSocketUrl);

    std::string host;
    std::string path;
    int port = 0;
    if (!ParseWebSocketUrl(webSocketUrl, host, port, path)) {
        return false;
    }

    HINTERNET session = WinHttpOpen(L"EZ PNGTuber/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        ReportLog("WinHttpOpen failed: " + std::to_string(GetLastError()));
        return false;
    }

    HINTERNET connection = WinHttpConnect(session, Widen(host).c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!connection) {
        ReportLog("WinHttpConnect failed: " + std::to_string(GetLastError()));
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"GET", Widen(path).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!request) {
        ReportLog("WinHttpOpenRequest failed: " + std::to_string(GetLastError()));
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
    const BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    const BOOL received = sent && WinHttpReceiveResponse(request, nullptr);
    if (!received) {
        ReportLog("WebSocket upgrade failed: " + std::to_string(GetLastError()));
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    webSocket_ = WinHttpWebSocketCompleteUpgrade(request, 0);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!webSocket_) {
        ReportLog("WinHttpWebSocketCompleteUpgrade failed: " + std::to_string(GetLastError()));
        return false;
    }

    DWORD timeoutMs = 1000;
    WinHttpSetOption(static_cast<HINTERNET>(webSocket_), WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    ReportStatus("Connected to browser DevTools.");
    return true;
#else
    (void)overlayUrl;
    return false;
#endif
}

bool StreamKitMonitor::SendEvaluateCommand()
{
#ifdef _WIN32
    if (!webSocket_) {
        return false;
    }

    const nlohmann::json command = {
        {"id", nextCommandId_++},
        {"method", "Runtime.evaluate"},
        {"params", {
            {"expression", OverlayScrapeScript},
            {"returnByValue", true},
            {"awaitPromise", false}
        }}
    };
    const auto body = command.dump();
    const auto result = WinHttpWebSocketSend(
        static_cast<HINTERNET>(webSocket_),
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size())
    );
    if (result != NO_ERROR) {
        ReportLog("WinHttpWebSocketSend failed: " + std::to_string(result));
    }
    return result == NO_ERROR;
#else
    return false;
#endif
}

void StreamKitMonitor::ReceiveLoopOnce()
{
#ifdef _WIN32
    if (!webSocket_) {
        return;
    }

    std::string message;
    std::array<char, 65536> buffer{};
    WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType;
    DWORD bytesRead = 0;

    const auto result = WinHttpWebSocketReceive(
        static_cast<HINTERNET>(webSocket_),
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        &bytesRead,
        &bufferType
    );

    if (result == ERROR_WINHTTP_TIMEOUT) {
        return;
    }

    if (result != NO_ERROR || bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
        ReportLog("WinHttpWebSocketReceive ended. result=" + std::to_string(result) + ", type=" + std::to_string(bufferType));
        running_ = false;
        return;
    }

    message.append(buffer.data(), bytesRead);
    if (!message.empty()) {
        HandleDevToolsMessage(message);
    }
#endif
}

void StreamKitMonitor::HandleDevToolsMessage(const std::string& message)
{
    const auto payload = nlohmann::json::parse(message, nullptr, false);
    if (payload.is_discarded()) {
        ReportLog("Invalid DevTools JSON: " + TruncateForLog(message));
        return;
    }
    if (payload.contains("error")) {
        ReportLog("DevTools error: " + payload.at("error").dump());
        return;
    }
    if (!payload.contains("result")) {
        return;
    }

    const auto value = payload["result"].value("result", nlohmann::json::object()).value("value", nlohmann::json::array());
    std::vector<DiscordVoiceUser> users;
    for (const auto& item : value) {
        DiscordVoiceUser user;
        user.id = item.value("id", item.value("name", ""));
        user.displayName = item.value("name", "");
        user.username = user.displayName;
        user.speaking = item.value("speaking", false);
        user.muted = item.value("muted", false);
        user.deafened = item.value("deafened", false);
        users.push_back(std::move(user));
    }

    if (onUsers_) {
        onUsers_(users);
    }
    if (lastReportedUserCount_ != static_cast<int>(users.size())) {
        lastReportedUserCount_ = static_cast<int>(users.size());
        ReportStatus("StreamKit overlay poll returned " + std::to_string(users.size()) + " visible user(s).");
        ReportLog("Scraped " + std::to_string(users.size()) + " visible user(s).");
    }
}

void StreamKitMonitor::ReportStatus(const std::string& status)
{
    if (onStatus_) {
        onStatus_(status);
    }
    ReportLog(status);
}

void StreamKitMonitor::ReportError(const std::string& error)
{
    if (onError_) {
        onError_(error);
    }
    ReportLog("ERROR: " + error);
}

void StreamKitMonitor::ReportLog(const std::string& message)
{
    if (onLog_) {
        onLog_(message);
    }
}

void StreamKitMonitor::CloseWebSocket()
{
#ifdef _WIN32
    if (webSocket_) {
        WinHttpWebSocketClose(static_cast<HINTERNET>(webSocket_), WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        WinHttpCloseHandle(static_cast<HINTERNET>(webSocket_));
        webSocket_ = nullptr;
    }
#endif
}

void StreamKitMonitor::StopBrowser()
{
#ifdef _WIN32
    if (browserProcess_) {
        TerminateProcess(static_cast<HANDLE>(browserProcess_), 0);
        CloseHandle(static_cast<HANDLE>(browserProcess_));
        browserProcess_ = nullptr;
    }
    if (browserThread_) {
        CloseHandle(static_cast<HANDLE>(browserThread_));
        browserThread_ = nullptr;
    }
#endif
}

std::string StreamKitMonitor::GetDevToolsList(int port)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {};
    }

    std::string response;
    const auto url = "http://127.0.0.1:" + std::to_string(port) + "/json/list";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendCurlData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 500L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ez-pngtuber/0.1.0");
    const auto result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return result == CURLE_OK ? response : std::string{};
}

std::string StreamKitMonitor::ExtractWebSocketDebuggerUrl(const std::string& jsonText, const std::string& overlayUrl, std::string& selectedPageUrl)
{
    const auto payload = nlohmann::json::parse(jsonText, nullptr, false);
    if (payload.is_discarded() || !payload.is_array()) {
        return {};
    }

    std::string fallback;
    for (const auto& page : payload) {
        if (page.value("type", "") != "page" || !page.contains("webSocketDebuggerUrl")) {
            continue;
        }

        const auto pageUrl = page.value("url", "");
        const auto webSocketUrl = page.value("webSocketDebuggerUrl", "");
        if (fallback.empty()) {
            fallback = webSocketUrl;
            selectedPageUrl = pageUrl;
        }

        if (pageUrl == overlayUrl || pageUrl.find("streamkit.discord.com") != std::string::npos) {
            selectedPageUrl = pageUrl;
            return webSocketUrl;
        }
    }
    return fallback;
}

bool StreamKitMonitor::ParseWebSocketUrl(const std::string& url, std::string& host, int& port, std::string& path)
{
    constexpr std::string_view prefix = "ws://";
    if (!url.starts_with(prefix)) {
        return false;
    }

    const auto withoutScheme = url.substr(prefix.size());
    const auto slash = withoutScheme.find('/');
    const auto colon = withoutScheme.find(':');
    if (slash == std::string::npos || colon == std::string::npos || colon > slash) {
        return false;
    }

    host = withoutScheme.substr(0, colon);
    port = std::stoi(withoutScheme.substr(colon + 1, slash - colon - 1));
    path = withoutScheme.substr(slash);
    return true;
}

