#pragma once

#include "DiscordModels.h"
#include "DiscordRpcClient.h"
#include "StreamKitMonitor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/listctrl.h>

class wxCommandEvent;
class wxCloseEvent;
class wxTextCtrl;
class wxStaticText;
class wxNotebook;
class wxCheckBox;
class wxSpinCtrl;
class wxFrame;

class MainWindow final : public wxFrame {
public:
    MainWindow();

private:
    void BuildMenu();
    void BuildDiscordPanel();
    void BuildStreamKitPanel(wxNotebook* notebook);
    void WireDiscordCallbacks();
    void WireStreamKitCallbacks();
    void OnDiscordConnect(wxCommandEvent& event);
    void OnDiscordDisconnect(wxCommandEvent& event);
    void OnRefreshServers(wxCommandEvent& event);
    void OnGuildSelected(wxCommandEvent& event);
    void OnMonitorChannel(wxCommandEvent& event);
    void OnStartStreamKit(wxCommandEvent& event);
    void OnStartStreamKitVisible(wxCommandEvent& event);
    void OnStopStreamKit(wxCommandEvent& event);
    void OnShowLogs(wxCommandEvent& event);
    void OnLogWindowClose(wxCloseEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void SetStatus(const std::string& status);
    void AppendLog(const std::string& message);
    void ShowLogsWindow();
    void SetGuilds(std::vector<DiscordGuild> guilds);
    void SetChannels(std::vector<DiscordChannel> channels);
    void SetVoiceUsers(std::vector<DiscordVoiceUser> users);
    void UpsertVoiceUser(DiscordVoiceUser user);
    void RemoveVoiceUser(const std::string& userId);
    void SetSpeaking(const std::string& userId, bool speaking);
    void RenderVoiceUsers();
    std::string SelectedGuildId() const;
    std::string SelectedChannelId() const;

    wxStaticText* statusLabel_ = nullptr;
    wxTextCtrl* clientIdText_ = nullptr;
    wxTextCtrl* clientSecretText_ = nullptr;
    wxTextCtrl* accessTokenText_ = nullptr;
    wxChoice* guildChoice_ = nullptr;
    wxChoice* channelChoice_ = nullptr;
    wxListCtrl* voiceUsersList_ = nullptr;
    wxStaticText* voiceUsersSummaryLabel_ = nullptr;
    wxTextCtrl* streamKitUrlText_ = nullptr;
    wxChoice* browserChoice_ = nullptr;
    wxTextCtrl* browserPathText_ = nullptr;
    wxCheckBox* showBrowserWindowCheck_ = nullptr;
    wxCheckBox* bypassLocalNetworkPromptCheck_ = nullptr;
    wxSpinCtrl* pollIntervalSpin_ = nullptr;
    wxFrame* logWindow_ = nullptr;
    wxTextCtrl* logText_ = nullptr;

    std::unique_ptr<DiscordRpcClient> discord_;
    std::unique_ptr<StreamKitMonitor> streamKit_;
    std::vector<StreamKitMonitor::BrowserCandidate> browsers_;
    std::vector<DiscordGuild> guilds_;
    std::vector<DiscordChannel> channels_;
    std::vector<std::string> logLines_;
    std::unordered_map<std::string, DiscordVoiceUser> voiceUsers_;
};
