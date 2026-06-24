#include "DiscordRpcClient.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>

#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
constexpr int DiscordOpcodeHandshake = 0;
constexpr int DiscordOpcodeFrame = 1;
constexpr int DiscordOpcodeClose = 2;
constexpr int DiscordOpcodePing = 3;
constexpr int DiscordOpcodePong = 4;
constexpr int DiscordVoiceChannelType = 2;

std::string JsonString(const nlohmann::json& value, const char* key)
{
    if (!value.contains(key) || value.at(key).is_null()) {
        return {};
    }
    return value.at(key).get<std::string>();
}

bool JsonBool(const nlohmann::json& value, const char* key)
{
    return value.contains(key) && value.at(key).is_boolean() && value.at(key).get<bool>();
}

int JsonInt(const nlohmann::json& value, const char* key, int fallback)
{
    return value.contains(key) && value.at(key).is_number_integer() ? value.at(key).get<int>() : fallback;
}

size_t AppendCurlData(char* data, size_t size, size_t nmemb, void* userData)
{
    auto* output = static_cast<std::string*>(userData);
    output->append(data, size * nmemb);
    return size * nmemb;
}
}

DiscordRpcClient::DiscordRpcClient()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

DiscordRpcClient::~DiscordRpcClient()
{
    Disconnect();
    curl_global_cleanup();
}

void DiscordRpcClient::SetErrorHandler(ErrorHandler handler) { onError_ = std::move(handler); }
void DiscordRpcClient::SetStatusHandler(StatusHandler handler) { onStatus_ = std::move(handler); }
void DiscordRpcClient::SetReadyHandler(ReadyHandler handler) { onReady_ = std::move(handler); }
void DiscordRpcClient::SetGuildsHandler(GuildsHandler handler) { onGuilds_ = std::move(handler); }
void DiscordRpcClient::SetChannelsHandler(ChannelsHandler handler) { onChannels_ = std::move(handler); }
void DiscordRpcClient::SetVoiceSnapshotHandler(VoiceUsersHandler handler) { onVoiceSnapshot_ = std::move(handler); }
void DiscordRpcClient::SetVoiceUserUpsertHandler(VoiceUserHandler handler) { onVoiceUserUpsert_ = std::move(handler); }
void DiscordRpcClient::SetVoiceUserDeleteHandler(VoiceUserIdHandler handler) { onVoiceUserDelete_ = std::move(handler); }
void DiscordRpcClient::SetSpeakingStartHandler(VoiceUserIdHandler handler) { onSpeakingStart_ = std::move(handler); }
void DiscordRpcClient::SetSpeakingStopHandler(VoiceUserIdHandler handler) { onSpeakingStop_ = std::move(handler); }

bool DiscordRpcClient::IsConnected() const
{
    return connected_;
}

bool DiscordRpcClient::Connect(std::string clientId)
{
    if (connected_) {
        return true;
    }

    clientId_ = std::move(clientId);
    if (clientId_.empty()) {
        ReportError("Discord client ID is required.");
        return false;
    }

    if (!OpenPipe()) {
        ReportError("Could not connect to Discord. Make sure the desktop client is running.");
        return false;
    }

    connected_ = true;
    const nlohmann::json handshake = {
        {"v", 1},
        {"client_id", clientId_}
    };

    if (!SendFrame(DiscordOpcodeHandshake, handshake)) {
        Disconnect();
        ReportError("Discord IPC handshake failed.");
        return false;
    }

    readThread_ = std::thread(&DiscordRpcClient::ReadLoop, this);
    ReportStatus("Connected to Discord IPC.");
    return true;
}

void DiscordRpcClient::Disconnect()
{
    connected_ = false;
    ClosePipe();
    if (readThread_.joinable()) {
        readThread_.join();
    }

    std::lock_guard lock(callbackMutex_);
    pendingResponses_.clear();
}

void DiscordRpcClient::AuthorizeAndAuthenticate(std::string clientSecret)
{
    nlohmann::json payload = {
        {"cmd", "AUTHORIZE"},
        {"args", {
            {"client_id", clientId_},
            {"scopes", nlohmann::json::array({"rpc", "identify"})}
        }}
    };

    SendCommand(std::move(payload), [this, clientSecret = std::move(clientSecret)](const nlohmann::json& response) {
        const auto code = JsonString(response.value("data", nlohmann::json::object()), "code");
        if (code.empty()) {
            ReportError("Discord did not return an authorization code.");
            return;
        }

        if (clientSecret.empty()) {
            ReportError("Discord authorized the app, but no client secret was provided to exchange the code.");
            return;
        }

        std::string accessToken;
        if (!ExchangeAuthCode(code, clientSecret, accessToken)) {
            return;
        }
        Authenticate(std::move(accessToken));
    });
}

void DiscordRpcClient::Authenticate(std::string accessToken)
{
    if (accessToken.empty()) {
        ReportError("Discord access token is required.");
        return;
    }

    nlohmann::json payload = {
        {"cmd", "AUTHENTICATE"},
        {"args", {{"access_token", std::move(accessToken)}}}
    };

    SendCommand(std::move(payload), [this](const nlohmann::json&) {
        ReportStatus("Discord RPC authenticated.");
        RequestGuilds();
    });
}

void DiscordRpcClient::RequestGuilds()
{
    SendCommand({{"cmd", "GET_GUILDS"}, {"args", nlohmann::json::object()}}, [this](const nlohmann::json& response) {
        std::vector<DiscordGuild> guilds;
        for (const auto& guild : response.value("data", nlohmann::json::object()).value("guilds", nlohmann::json::array())) {
            guilds.push_back({JsonString(guild, "id"), JsonString(guild, "name")});
        }
        if (onGuilds_) {
            onGuilds_(guilds);
        }
    });
}

void DiscordRpcClient::RequestChannels(std::string guildId)
{
    SendCommand({{"cmd", "GET_CHANNELS"}, {"args", {{"guild_id", std::move(guildId)}}}}, [this](const nlohmann::json& response) {
        std::vector<DiscordChannel> channels;
        for (const auto& channel : response.value("data", nlohmann::json::object()).value("channels", nlohmann::json::array())) {
            DiscordChannel parsed;
            parsed.id = JsonString(channel, "id");
            parsed.guildId = JsonString(channel, "guild_id");
            parsed.name = JsonString(channel, "name");
            parsed.type = JsonInt(channel, "type", 0);
            if (parsed.type == DiscordVoiceChannelType) {
                channels.push_back(std::move(parsed));
            }
        }
        if (onChannels_) {
            onChannels_(channels);
        }
    });
}

void DiscordRpcClient::MonitorVoiceChannel(std::string channelId)
{
    monitoredChannelId_ = std::move(channelId);
    if (monitoredChannelId_.empty()) {
        ReportError("Select a voice channel before monitoring.");
        return;
    }

    const auto args = nlohmann::json{{"channel_id", monitoredChannelId_}};
    for (const auto* eventName : {"VOICE_STATE_CREATE", "VOICE_STATE_UPDATE", "VOICE_STATE_DELETE", "SPEAKING_START", "SPEAKING_STOP"}) {
        SendCommand({{"cmd", "SUBSCRIBE"}, {"evt", eventName}, {"args", args}});
    }

    SendCommand({{"cmd", "GET_CHANNEL"}, {"args", {{"channel_id", monitoredChannelId_}}}}, [this](const nlohmann::json& response) {
        std::vector<DiscordVoiceUser> users;
        for (const auto& user : response.value("data", nlohmann::json::object()).value("voice_states", nlohmann::json::array())) {
            users.push_back(ParseVoiceUser(user));
        }
        if (onVoiceSnapshot_) {
            onVoiceSnapshot_(users);
        }
    });
}

bool DiscordRpcClient::OpenPipe()
{
#ifdef _WIN32
    for (int index = 0; index < 10; ++index) {
        const auto pipeName = "\\\\.\\pipe\\discord-ipc-" + std::to_string(index);
        pipeHandle_ = CreateFileA(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (pipeHandle_ != INVALID_HANDLE_VALUE) {
            return true;
        }
        pipeHandle_ = nullptr;
    }
#endif
    return false;
}

bool DiscordRpcClient::SendFrame(int opcode, const nlohmann::json& payload)
{
#ifdef _WIN32
    if (!pipeHandle_) {
        return false;
    }

    const auto body = payload.dump();
    std::vector<unsigned char> frame(8 + body.size());
    const auto op = static_cast<uint32_t>(opcode);
    const auto length = static_cast<uint32_t>(body.size());
    std::memcpy(frame.data(), &op, sizeof(op));
    std::memcpy(frame.data() + 4, &length, sizeof(length));
    std::memcpy(frame.data() + 8, body.data(), body.size());

    std::lock_guard lock(writeMutex_);
    DWORD bytesWritten = 0;
    return WriteFile(pipeHandle_, frame.data(), static_cast<DWORD>(frame.size()), &bytesWritten, nullptr) && bytesWritten == frame.size();
#else
    (void)opcode;
    (void)payload;
    return false;
#endif
}

void DiscordRpcClient::SendCommand(nlohmann::json payload, ResponseHandler responseHandler)
{
    const auto nonce = NextNonce();
    payload["nonce"] = nonce;
    if (responseHandler) {
        std::lock_guard lock(callbackMutex_);
        pendingResponses_[nonce] = std::move(responseHandler);
    }

    if (!SendFrame(DiscordOpcodeFrame, payload)) {
        ReportError("Failed to send Discord RPC command.");
    }
}

void DiscordRpcClient::ReadLoop()
{
#ifdef _WIN32
    while (connected_) {
        std::array<unsigned char, 8> header{};
        DWORD bytesRead = 0;
        if (!ReadFile(pipeHandle_, header.data(), static_cast<DWORD>(header.size()), &bytesRead, nullptr) || bytesRead != header.size()) {
            break;
        }

        uint32_t opcode = 0;
        uint32_t length = 0;
        std::memcpy(&opcode, header.data(), sizeof(opcode));
        std::memcpy(&length, header.data() + 4, sizeof(length));

        std::string body(length, '\0');
        if (length > 0 && (!ReadFile(pipeHandle_, body.data(), length, &bytesRead, nullptr) || bytesRead != length)) {
            break;
        }

        if (opcode == DiscordOpcodePing) {
            nlohmann::json ping = nlohmann::json::parse(body, nullptr, false);
            if (!ping.is_discarded()) {
                SendFrame(DiscordOpcodePong, ping);
            }
            continue;
        }

        if (opcode == DiscordOpcodeClose) {
            ReportError("Discord closed the RPC connection.");
            break;
        }

        if (opcode != DiscordOpcodeFrame) {
            continue;
        }

        auto payload = nlohmann::json::parse(body, nullptr, false);
        if (payload.is_discarded()) {
            ReportError("Discord sent invalid JSON.");
            continue;
        }
        HandlePayload(payload);
    }
#endif
    connected_ = false;
}

void DiscordRpcClient::HandlePayload(const nlohmann::json& payload)
{
    if (payload.value("evt", "") == "ERROR" || payload.value("cmd", "") == "ERROR") {
        const auto data = payload.value("data", nlohmann::json::object());
        std::ostringstream message;
        message << "Discord RPC error";
        if (data.contains("code")) {
            message << " " << data.at("code");
        }
        if (data.contains("message")) {
            message << ": " << data.at("message").get<std::string>();
        }
        ReportError(message.str());
        return;
    }

    if (payload.value("cmd", "") == "DISPATCH") {
        HandleDispatch(payload);
        return;
    }

    const auto nonce = JsonString(payload, "nonce");
    ResponseHandler handler;
    if (!nonce.empty()) {
        std::lock_guard lock(callbackMutex_);
        const auto found = pendingResponses_.find(nonce);
        if (found != pendingResponses_.end()) {
            handler = std::move(found->second);
            pendingResponses_.erase(found);
        }
    }

    if (handler) {
        handler(payload);
    }
}

void DiscordRpcClient::HandleDispatch(const nlohmann::json& payload)
{
    const auto eventName = payload.value("evt", "");
    const auto data = payload.value("data", nlohmann::json::object());

    if (eventName == "READY") {
        if (onReady_) {
            onReady_();
        }
        return;
    }

    if (eventName == "VOICE_STATE_CREATE" || eventName == "VOICE_STATE_UPDATE") {
        if (onVoiceUserUpsert_) {
            onVoiceUserUpsert_(ParseVoiceUser(data));
        }
        return;
    }

    if (eventName == "VOICE_STATE_DELETE") {
        if (onVoiceUserDelete_) {
            const auto user = data.value("user", nlohmann::json::object());
            onVoiceUserDelete_(JsonString(user, "id"));
        }
        return;
    }

    if (eventName == "SPEAKING_START") {
        if (onSpeakingStart_) {
            onSpeakingStart_(JsonString(data, "user_id"));
        }
        return;
    }

    if (eventName == "SPEAKING_STOP") {
        if (onSpeakingStop_) {
            onSpeakingStop_(JsonString(data, "user_id"));
        }
    }
}

bool DiscordRpcClient::ExchangeAuthCode(const std::string& code, const std::string& clientSecret, std::string& accessToken)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        ReportError("Could not initialize curl for Discord OAuth.");
        return false;
    }

    char* escapedCode = curl_easy_escape(curl, code.c_str(), static_cast<int>(code.size()));
    char* escapedClientId = curl_easy_escape(curl, clientId_.c_str(), static_cast<int>(clientId_.size()));
    char* escapedClientSecret = curl_easy_escape(curl, clientSecret.c_str(), static_cast<int>(clientSecret.size()));

    std::string response;
    std::ostringstream body;
    body << "grant_type=authorization_code"
         << "&code=" << (escapedCode ? escapedCode : "")
         << "&client_id=" << (escapedClientId ? escapedClientId : "")
         << "&client_secret=" << (escapedClientSecret ? escapedClientSecret : "");
    const auto postBody = body.str();

    curl_free(escapedCode);
    curl_free(escapedClientId);
    curl_free(escapedClientSecret);

    curl_easy_setopt(curl, CURLOPT_URL, "https://discord.com/api/oauth2/token");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendCurlData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ez-pngtuber/0.1.0");

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const auto result = curl_easy_perform(curl);
    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        ReportError(std::string("Discord OAuth request failed: ") + curl_easy_strerror(result));
        return false;
    }

    const auto payload = nlohmann::json::parse(response, nullptr, false);
    if (statusCode < 200 || statusCode >= 300 || payload.is_discarded()) {
        ReportError("Discord OAuth token exchange failed: " + response);
        return false;
    }

    accessToken = JsonString(payload, "access_token");
    if (accessToken.empty()) {
        ReportError("Discord OAuth response did not include an access token.");
        return false;
    }

    return true;
}

std::string DiscordRpcClient::NextNonce()
{
    return "ez-pngtuber-" + std::to_string(++nonceCounter_);
}

void DiscordRpcClient::ReportError(const std::string& message)
{
    if (onError_) {
        onError_(message);
    }
}

void DiscordRpcClient::ReportStatus(const std::string& message)
{
    if (onStatus_) {
        onStatus_(message);
    }
}

void DiscordRpcClient::ClosePipe()
{
#ifdef _WIN32
    if (pipeHandle_) {
        CancelIoEx(pipeHandle_, nullptr);
        CloseHandle(pipeHandle_);
        pipeHandle_ = nullptr;
    }
#endif
}

DiscordVoiceUser DiscordRpcClient::ParseVoiceUser(const nlohmann::json& data)
{
    const auto user = data.value("user", nlohmann::json::object());
    const auto state = data.value("voice_state", nlohmann::json::object());

    DiscordVoiceUser parsed;
    parsed.id = JsonString(user, "id");
    parsed.username = JsonString(user, "username");
    parsed.displayName = JsonString(data, "nick");
    if (parsed.displayName.empty()) {
        parsed.displayName = parsed.username;
    }
    parsed.muted = JsonBool(data, "mute") || JsonBool(state, "mute");
    parsed.deafened = JsonBool(state, "deaf");
    parsed.selfMuted = JsonBool(state, "self_mute");
    parsed.selfDeafened = JsonBool(state, "self_deaf");
    parsed.volume = JsonInt(data, "volume", 100);
    return parsed;
}
