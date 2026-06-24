#pragma once

#include <string>
#include <vector>

struct DiscordGuild {
    std::string id;
    std::string name;
};

struct DiscordChannel {
    std::string id;
    std::string guildId;
    std::string name;
    int type = 0;
};

struct DiscordVoiceUser {
    std::string id;
    std::string displayName;
    std::string username;
    bool muted = false;
    bool deafened = false;
    bool selfMuted = false;
    bool selfDeafened = false;
    bool speaking = false;
    int volume = 100;
};

