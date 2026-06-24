#include "MainWindow.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>
#include <wx/textctrl.h>

namespace {
enum MenuIds {
    IdDiscordConnect = wxID_HIGHEST + 1,
    IdDiscordDisconnect,
    IdRefreshServers,
    IdMonitorChannel,
    IdStartStreamKit,
    IdStartStreamKitVisible,
    IdStopStreamKit
};

wxString BoolText(bool value)
{
    return value ? "Yes" : "No";
}
}

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, "EZ PNGTuber 0.1.0", wxDefaultPosition, wxSize(980, 680)),
      discord_(std::make_unique<DiscordRpcClient>()),
      streamKit_(std::make_unique<StreamKitMonitor>())
{
    BuildMenu();
    BuildDiscordPanel();
    WireDiscordCallbacks();
    WireStreamKitCallbacks();

    CreateStatusBar();
    SetStatus("Ready. Paste a StreamKit overlay URL to monitor voice activity without Discord app credentials.");
    Centre();
}

void MainWindow::BuildMenu()
{
    auto* discordMenu = new wxMenu();
    discordMenu->Append(IdDiscordConnect, "&Connect / Authenticate...\tCtrl+D");
    discordMenu->Append(IdRefreshServers, "&Refresh Servers\tF5");
    discordMenu->Append(IdMonitorChannel, "&Monitor Selected Voice Channel\tCtrl+M");
    discordMenu->AppendSeparator();
    discordMenu->Append(IdDiscordDisconnect, "&Disconnect");

    auto* streamKitMenu = new wxMenu();
    streamKitMenu->Append(IdStartStreamKit, "&Start Overlay Monitor\tCtrl+K");
    streamKitMenu->Append(IdStopStreamKit, "S&top Overlay Monitor");

    auto* helpMenu = new wxMenu();
    helpMenu->Append(wxID_ABOUT, "&About");
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_EXIT, "E&xit");

    auto* menuBar = new wxMenuBar();
    menuBar->Append(streamKitMenu, "&StreamKit");
    menuBar->Append(discordMenu, "&Discord");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, &MainWindow::OnDiscordConnect, this, IdDiscordConnect);
    Bind(wxEVT_MENU, &MainWindow::OnDiscordDisconnect, this, IdDiscordDisconnect);
    Bind(wxEVT_MENU, &MainWindow::OnRefreshServers, this, IdRefreshServers);
    Bind(wxEVT_MENU, &MainWindow::OnMonitorChannel, this, IdMonitorChannel);
    Bind(wxEVT_MENU, &MainWindow::OnStartStreamKit, this, IdStartStreamKit);
    Bind(wxEVT_MENU, &MainWindow::OnStopStreamKit, this, IdStopStreamKit);
    Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainWindow::OnAbout, this, wxID_ABOUT);
}

void MainWindow::BuildDiscordPanel()
{
    auto* panel = new wxPanel(this);
    auto* outer = new wxBoxSizer(wxVERTICAL);

    auto* titleLabel = new wxStaticText(panel, wxID_ANY, "Discord Voice Monitor");
    auto titleFont = titleLabel->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 7);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleLabel->SetFont(titleFont);

    statusLabel_ = new wxStaticText(panel, wxID_ANY, "Connect to Discord, choose a server and voice channel, then start monitoring.");
    statusLabel_->Wrap(880);

    auto* notebook = new wxNotebook(panel, wxID_ANY);
    BuildStreamKitPanel(notebook);

    auto* rpcPanel = new wxPanel(notebook);
    auto* rpcOuter = new wxBoxSizer(wxVERTICAL);

    auto* authBox = new wxStaticBoxSizer(wxVERTICAL, rpcPanel, "Discord RPC");
    auto* authGrid = new wxFlexGridSizer(3, 2, 8, 10);
    authGrid->AddGrowableCol(1, 1);

    clientIdText_ = new wxTextCtrl(rpcPanel, wxID_ANY);
    clientSecretText_ = new wxTextCtrl(rpcPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    accessTokenText_ = new wxTextCtrl(rpcPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);

    authGrid->Add(new wxStaticText(rpcPanel, wxID_ANY, "Client ID"), 0, wxALIGN_CENTER_VERTICAL);
    authGrid->Add(clientIdText_, 1, wxEXPAND);
    authGrid->Add(new wxStaticText(rpcPanel, wxID_ANY, "Client Secret"), 0, wxALIGN_CENTER_VERTICAL);
    authGrid->Add(clientSecretText_, 1, wxEXPAND);
    authGrid->Add(new wxStaticText(rpcPanel, wxID_ANY, "Access Token"), 0, wxALIGN_CENTER_VERTICAL);
    authGrid->Add(accessTokenText_, 1, wxEXPAND);

    auto* authButtons = new wxBoxSizer(wxHORIZONTAL);
    auto* connectButton = new wxButton(rpcPanel, IdDiscordConnect, "Connect / Authenticate");
    auto* refreshButton = new wxButton(rpcPanel, IdRefreshServers, "Refresh Servers");
    auto* disconnectButton = new wxButton(rpcPanel, IdDiscordDisconnect, "Disconnect");
    connectButton->Bind(wxEVT_BUTTON, &MainWindow::OnDiscordConnect, this);
    refreshButton->Bind(wxEVT_BUTTON, &MainWindow::OnRefreshServers, this);
    disconnectButton->Bind(wxEVT_BUTTON, &MainWindow::OnDiscordDisconnect, this);
    authButtons->Add(connectButton, 0, wxRIGHT, 8);
    authButtons->Add(refreshButton, 0, wxRIGHT, 8);
    authButtons->Add(disconnectButton, 0);

    authBox->Add(authGrid, 0, wxEXPAND | wxALL, 10);
    authBox->Add(authButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* channelBox = new wxStaticBoxSizer(wxVERTICAL, rpcPanel, "Monitor");
    auto* channelGrid = new wxFlexGridSizer(2, 2, 8, 10);
    channelGrid->AddGrowableCol(1, 1);
    guildChoice_ = new wxChoice(rpcPanel, wxID_ANY);
    channelChoice_ = new wxChoice(rpcPanel, wxID_ANY);
    guildChoice_->Bind(wxEVT_CHOICE, &MainWindow::OnGuildSelected, this);

    channelGrid->Add(new wxStaticText(rpcPanel, wxID_ANY, "Server"), 0, wxALIGN_CENTER_VERTICAL);
    channelGrid->Add(guildChoice_, 1, wxEXPAND);
    channelGrid->Add(new wxStaticText(rpcPanel, wxID_ANY, "Voice Channel"), 0, wxALIGN_CENTER_VERTICAL);
    channelGrid->Add(channelChoice_, 1, wxEXPAND);

    auto* monitorButton = new wxButton(rpcPanel, IdMonitorChannel, "Monitor Voice Channel");
    monitorButton->Bind(wxEVT_BUTTON, &MainWindow::OnMonitorChannel, this);
    channelBox->Add(channelGrid, 0, wxEXPAND | wxALL, 10);
    channelBox->Add(monitorButton, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    voiceUsersList_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    voiceUsersList_->AppendColumn("User", wxLIST_FORMAT_LEFT, 220);
    voiceUsersList_->AppendColumn("Speaking", wxLIST_FORMAT_LEFT, 90);
    voiceUsersList_->AppendColumn("Muted", wxLIST_FORMAT_LEFT, 90);
    voiceUsersList_->AppendColumn("Deafened", wxLIST_FORMAT_LEFT, 90);
    voiceUsersList_->AppendColumn("Self Muted", wxLIST_FORMAT_LEFT, 100);
    voiceUsersList_->AppendColumn("Self Deafened", wxLIST_FORMAT_LEFT, 110);
    voiceUsersList_->AppendColumn("Volume", wxLIST_FORMAT_RIGHT, 80);

    logText_ = new wxTextCtrl(
        panel,
        wxID_ANY,
        "",
        wxDefaultPosition,
        wxSize(-1, 140),
        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP
    );

    outer->Add(titleLabel, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    outer->Add(statusLabel_, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 16);
    rpcOuter->Add(authBox, 0, wxEXPAND | wxALL, 10);
    rpcOuter->Add(channelBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    rpcPanel->SetSizer(rpcOuter);
    notebook->AddPage(rpcPanel, "Advanced RPC");

    outer->Add(notebook, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    outer->Add(voiceUsersList_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    outer->Add(new wxStaticText(panel, wxID_ANY, "Logs"), 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    outer->Add(logText_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    panel->SetSizer(outer);
}

void MainWindow::BuildStreamKitPanel(wxNotebook* notebook)
{
    auto* panel = new wxPanel(notebook);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* box = new wxStaticBoxSizer(wxVERTICAL, panel, "StreamKit Overlay");
    auto* grid = new wxFlexGridSizer(3, 2, 8, 10);
    grid->AddGrowableCol(1, 1);

    streamKitUrlText_ = new wxTextCtrl(panel, wxID_ANY);
    browserChoice_ = new wxChoice(panel, wxID_ANY);
    browserPathText_ = new wxTextCtrl(panel, wxID_ANY);
    showBrowserWindowCheck_ = new wxCheckBox(panel, wxID_ANY, "Show browser window");

    browsers_ = StreamKitMonitor::DetectBrowsers();
    for (const auto& browser : browsers_) {
        browserChoice_->Append(browser.name + " - " + browser.path);
    }
    if (!browsers_.empty()) {
        browserChoice_->SetSelection(0);
        browserPathText_->SetValue(browsers_.front().path);
    }
    browserChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        const auto selection = browserChoice_->GetSelection();
        if (selection != wxNOT_FOUND && static_cast<size_t>(selection) < browsers_.size()) {
            browserPathText_->SetValue(browsers_[selection].path);
        }
    });

    grid->Add(new wxStaticText(panel, wxID_ANY, "Overlay URL"), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(streamKitUrlText_, 1, wxEXPAND);
    grid->Add(new wxStaticText(panel, wxID_ANY, "Detected Browser"), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(browserChoice_, 1, wxEXPAND);
    grid->Add(new wxStaticText(panel, wxID_ANY, "Browser Path"), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(browserPathText_, 1, wxEXPAND);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* startButton = new wxButton(panel, IdStartStreamKit, "Start Overlay Monitor");
    auto* startVisibleButton = new wxButton(panel, IdStartStreamKitVisible, "Start With Browser Window");
    auto* stopButton = new wxButton(panel, IdStopStreamKit, "Stop");
    startButton->Bind(wxEVT_BUTTON, &MainWindow::OnStartStreamKit, this);
    startVisibleButton->Bind(wxEVT_BUTTON, &MainWindow::OnStartStreamKitVisible, this);
    stopButton->Bind(wxEVT_BUTTON, &MainWindow::OnStopStreamKit, this);
    buttons->Add(startButton, 0, wxRIGHT, 8);
    buttons->Add(startVisibleButton, 0, wxRIGHT, 8);
    buttons->Add(stopButton, 0);

    box->Add(grid, 0, wxEXPAND | wxALL, 10);
    box->Add(showBrowserWindowCheck_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    box->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    outer->Add(box, 0, wxEXPAND | wxALL, 10);
    panel->SetSizer(outer);
    notebook->AddPage(panel, "StreamKit Overlay");
}

void MainWindow::WireDiscordCallbacks()
{
    discord_->SetStatusHandler([this](const std::string& status) {
        CallAfter([this, status] { SetStatus(status); });
    });
    discord_->SetErrorHandler([this](const std::string& error) {
        CallAfter([this, error] { SetStatus(error); });
    });
    discord_->SetReadyHandler([this] {
        CallAfter([this] { SetStatus("Discord IPC ready. Waiting for authentication."); });
    });
    discord_->SetGuildsHandler([this](const std::vector<DiscordGuild>& guilds) {
        CallAfter([this, guilds] { SetGuilds(guilds); });
    });
    discord_->SetChannelsHandler([this](const std::vector<DiscordChannel>& channels) {
        CallAfter([this, channels] { SetChannels(channels); });
    });
    discord_->SetVoiceSnapshotHandler([this](const std::vector<DiscordVoiceUser>& users) {
        CallAfter([this, users] { SetVoiceUsers(users); });
    });
    discord_->SetVoiceUserUpsertHandler([this](const DiscordVoiceUser& user) {
        CallAfter([this, user] { UpsertVoiceUser(user); });
    });
    discord_->SetVoiceUserDeleteHandler([this](const std::string& userId) {
        CallAfter([this, userId] { RemoveVoiceUser(userId); });
    });
    discord_->SetSpeakingStartHandler([this](const std::string& userId) {
        CallAfter([this, userId] { SetSpeaking(userId, true); });
    });
    discord_->SetSpeakingStopHandler([this](const std::string& userId) {
        CallAfter([this, userId] { SetSpeaking(userId, false); });
    });
}

void MainWindow::WireStreamKitCallbacks()
{
    streamKit_->SetStatusHandler([this](const std::string& status) {
        CallAfter([this, status] { SetStatus(status); });
    });
    streamKit_->SetErrorHandler([this](const std::string& error) {
        CallAfter([this, error] { SetStatus(error); });
    });
    streamKit_->SetLogHandler([this](const std::string& message) {
        CallAfter([this, message] { AppendLog(message); });
    });
    streamKit_->SetUsersHandler([this](const std::vector<DiscordVoiceUser>& users) {
        CallAfter([this, users] { SetVoiceUsers(users); });
    });
}

void MainWindow::OnDiscordConnect(wxCommandEvent&)
{
    const auto clientId = clientIdText_->GetValue().ToStdString();
    if (!discord_->Connect(clientId)) {
        return;
    }

    const auto accessToken = accessTokenText_->GetValue().ToStdString();
    if (!accessToken.empty()) {
        discord_->Authenticate(accessToken);
        return;
    }

    discord_->AuthorizeAndAuthenticate(clientSecretText_->GetValue().ToStdString());
}

void MainWindow::OnDiscordDisconnect(wxCommandEvent&)
{
    discord_->Disconnect();
    guilds_.clear();
    channels_.clear();
    voiceUsers_.clear();
    guildChoice_->Clear();
    channelChoice_->Clear();
    RenderVoiceUsers();
    SetStatus("Disconnected from Discord.");
}

void MainWindow::OnRefreshServers(wxCommandEvent&)
{
    discord_->RequestGuilds();
}

void MainWindow::OnGuildSelected(wxCommandEvent&)
{
    const auto guildId = SelectedGuildId();
    channelChoice_->Clear();
    channels_.clear();
    if (!guildId.empty()) {
        discord_->RequestChannels(guildId);
    }
}

void MainWindow::OnMonitorChannel(wxCommandEvent&)
{
    const auto channelId = SelectedChannelId();
    voiceUsers_.clear();
    RenderVoiceUsers();
    discord_->MonitorVoiceChannel(channelId);
    SetStatus("Monitoring selected Discord voice channel.");
}

void MainWindow::OnStartStreamKit(wxCommandEvent&)
{
    voiceUsers_.clear();
    RenderVoiceUsers();
    streamKit_->Start(
        browserPathText_->GetValue().ToStdString(),
        streamKitUrlText_->GetValue().ToStdString(),
        showBrowserWindowCheck_->GetValue()
    );
}

void MainWindow::OnStartStreamKitVisible(wxCommandEvent&)
{
    showBrowserWindowCheck_->SetValue(true);
    voiceUsers_.clear();
    RenderVoiceUsers();
    streamKit_->Start(
        browserPathText_->GetValue().ToStdString(),
        streamKitUrlText_->GetValue().ToStdString(),
        true
    );
}

void MainWindow::OnStopStreamKit(wxCommandEvent&)
{
    streamKit_->Stop();
    SetStatus("Stopped StreamKit overlay monitor.");
}

void MainWindow::OnExit(wxCommandEvent&)
{
    streamKit_->Stop();
    discord_->Disconnect();
    Close(true);
}

void MainWindow::OnAbout(wxCommandEvent&)
{
    wxMessageBox(
        "EZ PNGTuber Discord voice monitor prototype.\n\nUses Discord local RPC IPC to list servers, voice channels, and voice activity events.",
        "About EZ PNGTuber",
        wxOK | wxICON_INFORMATION,
        this
    );
}

void MainWindow::SetStatus(const std::string& status)
{
    statusLabel_->SetLabel(status);
    statusLabel_->Wrap(880);
    SetStatusText(status);
    Layout();
}

void MainWindow::AppendLog(const std::string& message)
{
    if (!logText_) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream line;
    line << std::put_time(&localTime, "%H:%M:%S") << "  " << message << "\n";
    logText_->AppendText(line.str());
}

void MainWindow::SetGuilds(std::vector<DiscordGuild> guilds)
{
    guilds_ = std::move(guilds);
    guildChoice_->Clear();
    for (const auto& guild : guilds_) {
        guildChoice_->Append(guild.name);
    }
    if (!guilds_.empty()) {
        guildChoice_->SetSelection(0);
        discord_->RequestChannels(guilds_.front().id);
    }
    SetStatus("Loaded Discord servers.");
}

void MainWindow::SetChannels(std::vector<DiscordChannel> channels)
{
    channels_ = std::move(channels);
    channelChoice_->Clear();
    for (const auto& channel : channels_) {
        channelChoice_->Append(channel.name);
    }
    if (!channels_.empty()) {
        channelChoice_->SetSelection(0);
    }
    SetStatus("Loaded voice channels.");
}

void MainWindow::SetVoiceUsers(std::vector<DiscordVoiceUser> users)
{
    voiceUsers_.clear();
    for (auto& user : users) {
        voiceUsers_[user.id] = std::move(user);
    }
    RenderVoiceUsers();
}

void MainWindow::UpsertVoiceUser(DiscordVoiceUser user)
{
    const auto existing = voiceUsers_.find(user.id);
    if (existing != voiceUsers_.end()) {
        user.speaking = existing->second.speaking;
    }
    voiceUsers_[user.id] = std::move(user);
    RenderVoiceUsers();
}

void MainWindow::RemoveVoiceUser(const std::string& userId)
{
    voiceUsers_.erase(userId);
    RenderVoiceUsers();
}

void MainWindow::SetSpeaking(const std::string& userId, bool speaking)
{
    const auto found = voiceUsers_.find(userId);
    if (found != voiceUsers_.end()) {
        found->second.speaking = speaking;
        RenderVoiceUsers();
    }
}

void MainWindow::RenderVoiceUsers()
{
    std::vector<DiscordVoiceUser> users;
    users.reserve(voiceUsers_.size());
    for (const auto& [_, user] : voiceUsers_) {
        users.push_back(user);
    }
    std::sort(users.begin(), users.end(), [](const auto& left, const auto& right) {
        return left.displayName < right.displayName;
    });

    voiceUsersList_->DeleteAllItems();
    for (const auto& user : users) {
        const long row = voiceUsersList_->InsertItem(voiceUsersList_->GetItemCount(), user.displayName);
        voiceUsersList_->SetItem(row, 1, BoolText(user.speaking));
        voiceUsersList_->SetItem(row, 2, BoolText(user.muted));
        voiceUsersList_->SetItem(row, 3, BoolText(user.deafened));
        voiceUsersList_->SetItem(row, 4, BoolText(user.selfMuted));
        voiceUsersList_->SetItem(row, 5, BoolText(user.selfDeafened));
        voiceUsersList_->SetItem(row, 6, wxString::Format("%d", user.volume));
    }
}

std::string MainWindow::SelectedGuildId() const
{
    const auto selection = guildChoice_->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<size_t>(selection) >= guilds_.size()) {
        return {};
    }
    return guilds_[selection].id;
}

std::string MainWindow::SelectedChannelId() const
{
    const auto selection = channelChoice_->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<size_t>(selection) >= channels_.size()) {
        return {};
    }
    return channels_[selection].id;
}
