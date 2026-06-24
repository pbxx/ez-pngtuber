#pragma once

#include "DiscordModels.h"
#include "GroupStore.h"
#include "StreamKitMonitor.h"

#include <memory>
#include <optional>
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
class wxButton;
class wxSpinCtrl;
class wxFrame;
class wxListBox;

class MainWindow final : public wxFrame {
public:
    MainWindow();

private:
    void BuildMenu();
    void BuildMainPanel();
    void BuildStreamKitPanel(wxNotebook* notebook);
    void BuildPngTuberPanel(wxNotebook* notebook);
    void LoadGroups();
    void LoadPngTubersForSelectedGroup();
    void SelectGroupById(int groupId);
    void ApplyGroupToControls(const StreamKitGroup& group);
    StreamKitGroup CollectGroupFromControls() const;
    void ApplyPngTuberToControls(const PngTuberConfig& pngTuber);
    PngTuberConfig CollectPngTuberFromControls() const;
    bool SaveSelectedGroupSettings(bool announceSuccess);
    bool SaveSelectedPngTuber(bool announceSuccess);
    std::optional<int> GetSelectedGroupId() const;
    std::optional<int> GetSelectedPngTuberId() const;
    void SelectPngTuberById(int pngTuberId);
    void ClearPngTuberControls();
    void BrowseForImage(wxTextCtrl* target);
    void WireStreamKitCallbacks();
    void OnGroupSelected(wxCommandEvent& event);
    void OnCreateGroup(wxCommandEvent& event);
    void OnRenameGroup(wxCommandEvent& event);
    void OnDeleteGroup(wxCommandEvent& event);
    void OnSaveGroup(wxCommandEvent& event);
    void OnPngTuberSelected(wxCommandEvent& event);
    void OnCreatePngTuber(wxCommandEvent& event);
    void OnDeletePngTuber(wxCommandEvent& event);
    void OnSavePngTuber(wxCommandEvent& event);
    void OnMainWindowClose(wxCloseEvent& event);
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
    wxChoice* groupChoice_ = nullptr;
    wxChoice* browserChoice_ = nullptr;
    wxTextCtrl* browserPathText_ = nullptr;
    wxCheckBox* showBrowserWindowCheck_ = nullptr;
    wxCheckBox* bypassLocalNetworkPromptCheck_ = nullptr;
    wxSpinCtrl* pollIntervalSpin_ = nullptr;
    wxButton* saveGroupButton_ = nullptr;
    wxStaticText* pngTuberGroupLabel_ = nullptr;
    wxListBox* pngTuberList_ = nullptr;
    wxTextCtrl* pngTuberNameText_ = nullptr;
    wxTextCtrl* closedMouthOpenEyesPathText_ = nullptr;
    wxTextCtrl* closedMouthClosedEyesPathText_ = nullptr;
    wxTextCtrl* openMouthOpenEyesPathText_ = nullptr;
    wxTextCtrl* openMouthClosedEyesPathText_ = nullptr;
    wxTextCtrl* mutePathText_ = nullptr;
    wxFrame* logWindow_ = nullptr;
    wxTextCtrl* logText_ = nullptr;

    std::unique_ptr<GroupStore> groupStore_;
    std::unique_ptr<StreamKitMonitor> streamKit_;
    std::vector<StreamKitGroup> groups_;
    std::vector<PngTuberConfig> pngTubers_;
    std::vector<StreamKitMonitor::BrowserCandidate> browsers_;
    std::vector<std::string> logLines_;
    std::unordered_map<std::string, DiscordVoiceUser> voiceUsers_;
};
