#pragma once

#include "DiscordModels.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

class DiscordRpcClient final {
public:
    using ErrorHandler = std::function<void(const std::string&)>;
    using StatusHandler = std::function<void(const std::string&)>;
    using ReadyHandler = std::function<void()>;
    using GuildsHandler = std::function<void(const std::vector<DiscordGuild>&)>;
    using ChannelsHandler = std::function<void(const std::vector<DiscordChannel>&)>;
    using VoiceUsersHandler = std::function<void(const std::vector<DiscordVoiceUser>&)>;
    using VoiceUserHandler = std::function<void(const DiscordVoiceUser&)>;
    using VoiceUserIdHandler = std::function<void(const std::string&)>;

    DiscordRpcClient();
    ~DiscordRpcClient();

    DiscordRpcClient(const DiscordRpcClient&) = delete;
    DiscordRpcClient& operator=(const DiscordRpcClient&) = delete;

    void SetErrorHandler(ErrorHandler handler);
    void SetStatusHandler(StatusHandler handler);
    void SetReadyHandler(ReadyHandler handler);
    void SetGuildsHandler(GuildsHandler handler);
    void SetChannelsHandler(ChannelsHandler handler);
    void SetVoiceSnapshotHandler(VoiceUsersHandler handler);
    void SetVoiceUserUpsertHandler(VoiceUserHandler handler);
    void SetVoiceUserDeleteHandler(VoiceUserIdHandler handler);
    void SetSpeakingStartHandler(VoiceUserIdHandler handler);
    void SetSpeakingStopHandler(VoiceUserIdHandler handler);

    bool IsConnected() const;
    bool Connect(std::string clientId);
    void Disconnect();

    void AuthorizeAndAuthenticate(std::string clientSecret);
    void Authenticate(std::string accessToken);
    void RequestGuilds();
    void RequestChannels(std::string guildId);
    void MonitorVoiceChannel(std::string channelId);

private:
    using ResponseHandler = std::function<void(const nlohmann::json&)>;

    bool OpenPipe();
    bool SendFrame(int opcode, const nlohmann::json& payload);
    void SendCommand(nlohmann::json payload, ResponseHandler responseHandler = {});
    void ReadLoop();
    void HandlePayload(const nlohmann::json& payload);
    void HandleDispatch(const nlohmann::json& payload);
    bool ExchangeAuthCode(const std::string& code, const std::string& clientSecret, std::string& accessToken);
    std::string NextNonce();
    void ReportError(const std::string& message);
    void ReportStatus(const std::string& message);
    void ClosePipe();

    static DiscordVoiceUser ParseVoiceUser(const nlohmann::json& data);

    std::string clientId_;
    std::atomic_bool connected_ = false;
    std::atomic_uint nonceCounter_ = 0;
    std::thread readThread_;
    std::mutex writeMutex_;
    std::mutex callbackMutex_;
    std::unordered_map<std::string, ResponseHandler> pendingResponses_;
    std::string monitoredChannelId_;

    ErrorHandler onError_;
    StatusHandler onStatus_;
    ReadyHandler onReady_;
    GuildsHandler onGuilds_;
    ChannelsHandler onChannels_;
    VoiceUsersHandler onVoiceSnapshot_;
    VoiceUserHandler onVoiceUserUpsert_;
    VoiceUserIdHandler onVoiceUserDelete_;
    VoiceUserIdHandler onSpeakingStart_;
    VoiceUserIdHandler onSpeakingStop_;

#ifdef _WIN32
    void* pipeHandle_ = nullptr;
#endif
};

