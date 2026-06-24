#pragma once

#include "DiscordModels.h"
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
    void BuildMainPanel();
    void BuildStreamKitPanel(wxNotebook* notebook);
    void WireStreamKitCallbacks();
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
    void SetVoiceUsers(std::vector<DiscordVoiceUser> users);
    void RenderVoiceUsers();

    wxStaticText* statusLabel_ = nullptr;
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

    std::unique_ptr<StreamKitMonitor> streamKit_;
    std::vector<StreamKitMonitor::BrowserCandidate> browsers_;
    std::vector<std::string> logLines_;
    std::unordered_map<std::string, DiscordVoiceUser> voiceUsers_;
};
