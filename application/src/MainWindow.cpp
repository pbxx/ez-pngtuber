#include "MainWindow.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/filedlg.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

namespace {
enum MenuIds {
    IdStartStreamKit = wxID_HIGHEST + 1,
    IdStartStreamKitVisible,
    IdStopStreamKit,
    IdShowLogs,
    IdGroupChoice,
    IdCreateGroup,
    IdRenameGroup,
    IdDeleteGroup,
    IdSaveGroup,
    IdPngTuberList,
    IdCreatePngTuber,
    IdDeletePngTuber,
    IdSavePngTuber
};

wxString BoolText(bool value)
{
    return value ? "Yes" : "No";
}

wxString SpeakingText(bool value)
{
    return value ? "Speaking" : "Silent";
}
}

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, "EZ PNGTuber 0.1.0", wxDefaultPosition, wxSize(980, 680)),
      groupStore_(std::make_unique<GroupStore>()),
      streamKit_(std::make_unique<StreamKitMonitor>())
{
    Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnMainWindowClose, this);
    BuildMenu();
    BuildMainPanel();
    WireStreamKitCallbacks();

    CreateStatusBar();
    SetStatus("Ready");
    Centre();
}

void MainWindow::BuildMenu()
{
    auto* streamKitMenu = new wxMenu();
    streamKitMenu->Append(IdStartStreamKit, "&Start Overlay Monitor\tCtrl+K");
    streamKitMenu->Append(IdStopStreamKit, "S&top Overlay Monitor");
    streamKitMenu->AppendSeparator();
    streamKitMenu->Append(IdShowLogs, "Show &Logs");

    auto* helpMenu = new wxMenu();
    helpMenu->Append(wxID_ABOUT, "&About");
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_EXIT, "E&xit");

    auto* menuBar = new wxMenuBar();
    menuBar->Append(streamKitMenu, "&StreamKit");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, &MainWindow::OnStartStreamKit, this, IdStartStreamKit);
    Bind(wxEVT_MENU, &MainWindow::OnStopStreamKit, this, IdStopStreamKit);
    Bind(wxEVT_MENU, &MainWindow::OnShowLogs, this, IdShowLogs);
    Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainWindow::OnAbout, this, wxID_ABOUT);
}

void MainWindow::BuildMainPanel()
{
    auto* panel = new wxPanel(this);
    auto* outer = new wxBoxSizer(wxVERTICAL);

    auto* notebook = new wxNotebook(panel, wxID_ANY);
    BuildStreamKitPanel(notebook);
    BuildPngTuberPanel(notebook);
    LoadGroups();

    outer->Add(notebook, 1, wxEXPAND | wxALL, 8);
    panel->SetSizer(outer);
}

void MainWindow::BuildStreamKitPanel(wxNotebook* notebook)
{
    auto* panel = new wxPanel(notebook);
    auto* outer = new wxBoxSizer(wxVERTICAL);

    auto* groupsBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Groups");
    auto* groupsRow = new wxBoxSizer(wxHORIZONTAL);
    groupChoice_ = new wxChoice(panel, IdGroupChoice);
    auto* createGroupButton = new wxButton(panel, IdCreateGroup, "New Group");
    auto* renameGroupButton = new wxButton(panel, IdRenameGroup, "Rename");
    auto* deleteGroupButton = new wxButton(panel, IdDeleteGroup, "Delete");
    saveGroupButton_ = new wxButton(panel, IdSaveGroup, "Save Group");

    groupChoice_->Bind(wxEVT_CHOICE, &MainWindow::OnGroupSelected, this);
    createGroupButton->Bind(wxEVT_BUTTON, &MainWindow::OnCreateGroup, this);
    renameGroupButton->Bind(wxEVT_BUTTON, &MainWindow::OnRenameGroup, this);
    deleteGroupButton->Bind(wxEVT_BUTTON, &MainWindow::OnDeleteGroup, this);
    saveGroupButton_->Bind(wxEVT_BUTTON, &MainWindow::OnSaveGroup, this);

    groupsRow->Add(new wxStaticText(panel, wxID_ANY, "Active Group"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    groupsRow->Add(groupChoice_, 1, wxRIGHT, 8);
    groupsRow->Add(createGroupButton, 0, wxRIGHT, 8);
    groupsRow->Add(renameGroupButton, 0, wxRIGHT, 8);
    groupsRow->Add(deleteGroupButton, 0, wxRIGHT, 8);
    groupsRow->Add(saveGroupButton_, 0);
    groupsBox->Add(groupsRow, 0, wxEXPAND | wxALL, 8);

    auto* settingsBox = new wxStaticBoxSizer(wxVERTICAL, panel, "StreamKit Overlay");
    auto* grid = new wxFlexGridSizer(4, 2, 6, 8);
    grid->AddGrowableCol(1, 1);

    streamKitUrlText_ = new wxTextCtrl(panel, wxID_ANY);
    browserChoice_ = new wxChoice(panel, wxID_ANY);
    browserPathText_ = new wxTextCtrl(panel, wxID_ANY);
    pollIntervalSpin_ = new wxSpinCtrl(panel, wxID_ANY);
    pollIntervalSpin_->SetRange(50, 5000);
    pollIntervalSpin_->SetValue(250);
    showBrowserWindowCheck_ = new wxCheckBox(panel, wxID_ANY, "Show browser window");
    bypassLocalNetworkPromptCheck_ = new wxCheckBox(panel, wxID_ANY, "Bypass local app prompt");
    bypassLocalNetworkPromptCheck_->SetValue(true);

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
    grid->Add(new wxStaticText(panel, wxID_ANY, "Poll Interval (ms)"), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(pollIntervalSpin_, 0, wxALIGN_LEFT);

    auto* checkboxes = new wxBoxSizer(wxHORIZONTAL);
    checkboxes->Add(showBrowserWindowCheck_, 0, wxRIGHT, 16);
    checkboxes->Add(bypassLocalNetworkPromptCheck_, 0);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* startButton = new wxButton(panel, IdStartStreamKit, "Start Overlay Monitor");
    auto* startVisibleButton = new wxButton(panel, IdStartStreamKitVisible, "Start With Browser Window");
    auto* stopButton = new wxButton(panel, IdStopStreamKit, "Stop");
    auto* logsButton = new wxButton(panel, IdShowLogs, "Show Logs");
    startButton->Bind(wxEVT_BUTTON, &MainWindow::OnStartStreamKit, this);
    startVisibleButton->Bind(wxEVT_BUTTON, &MainWindow::OnStartStreamKitVisible, this);
    stopButton->Bind(wxEVT_BUTTON, &MainWindow::OnStopStreamKit, this);
    logsButton->Bind(wxEVT_BUTTON, &MainWindow::OnShowLogs, this);
    buttons->Add(startButton, 0, wxRIGHT, 8);
    buttons->Add(startVisibleButton, 0, wxRIGHT, 8);
    buttons->Add(stopButton, 0, wxRIGHT, 8);
    buttons->Add(logsButton, 0);

    settingsBox->Add(grid, 0, wxEXPAND | wxALL, 8);
    settingsBox->Add(checkboxes, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    settingsBox->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* usersBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Users in Call");
    voiceUsersSummaryLabel_ = new wxStaticText(panel, wxID_ANY, "0 users in call");
    voiceUsersList_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    voiceUsersList_->AppendColumn("User", wxLIST_FORMAT_LEFT, 240);
    voiceUsersList_->AppendColumn("Speaking", wxLIST_FORMAT_LEFT, 110);
    voiceUsersList_->AppendColumn("Muted", wxLIST_FORMAT_LEFT, 90);
    voiceUsersList_->AppendColumn("Deafened", wxLIST_FORMAT_LEFT, 90);
    voiceUsersList_->AppendColumn("Self Muted", wxLIST_FORMAT_LEFT, 100);
    voiceUsersList_->AppendColumn("Self Deafened", wxLIST_FORMAT_LEFT, 110);
    voiceUsersList_->AppendColumn("Volume", wxLIST_FORMAT_RIGHT, 80);

    usersBox->Add(voiceUsersSummaryLabel_, 0, wxEXPAND | wxALL, 8);
    usersBox->Add(voiceUsersList_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    outer->Add(groupsBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    outer->Add(settingsBox, 0, wxEXPAND | wxALL, 8);
    outer->Add(usersBox, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    panel->SetSizer(outer);
    notebook->AddPage(panel, "StreamKit Overlay");
}

void MainWindow::BuildPngTuberPanel(wxNotebook* notebook)
{
    auto* panel = new wxPanel(notebook);
    auto* outer = new wxBoxSizer(wxVERTICAL);

    auto* headerBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Group Scope");
    pngTuberGroupLabel_ = new wxStaticText(panel, wxID_ANY, "Active group: none");
    headerBox->Add(
        pngTuberGroupLabel_,
        0,
        wxEXPAND | wxALL,
        8
    );

    auto* content = new wxBoxSizer(wxHORIZONTAL);

    auto* listBox = new wxStaticBoxSizer(wxVERTICAL, panel, "PNGTubers");
    pngTuberList_ = new wxListBox(panel, IdPngTuberList);
    auto* pngTuberButtons = new wxBoxSizer(wxHORIZONTAL);
    auto* newPngTuberButton = new wxButton(panel, IdCreatePngTuber, "New PNGTuber");
    auto* deletePngTuberButton = new wxButton(panel, IdDeletePngTuber, "Delete");
    newPngTuberButton->Bind(wxEVT_BUTTON, &MainWindow::OnCreatePngTuber, this);
    deletePngTuberButton->Bind(wxEVT_BUTTON, &MainWindow::OnDeletePngTuber, this);
    pngTuberList_->Bind(wxEVT_LISTBOX, &MainWindow::OnPngTuberSelected, this);
    pngTuberButtons->Add(newPngTuberButton, 0, wxRIGHT, 8);
    pngTuberButtons->Add(deletePngTuberButton, 0);
    listBox->Add(pngTuberList_, 1, wxEXPAND | wxALL, 8);
    listBox->Add(pngTuberButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* editorBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Configurator");
    auto* grid = new wxFlexGridSizer(6, 3, 6, 8);
    grid->AddGrowableCol(1, 1);

    pngTuberNameText_ = new wxTextCtrl(panel, wxID_ANY);
    closedMouthOpenEyesPathText_ = new wxTextCtrl(panel, wxID_ANY);
    closedMouthClosedEyesPathText_ = new wxTextCtrl(panel, wxID_ANY);
    openMouthOpenEyesPathText_ = new wxTextCtrl(panel, wxID_ANY);
    openMouthClosedEyesPathText_ = new wxTextCtrl(panel, wxID_ANY);
    mutePathText_ = new wxTextCtrl(panel, wxID_ANY);

    auto addPathRow = [&](const wxString& label, wxTextCtrl*& textCtrl) {
        grid->Add(new wxStaticText(panel, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(textCtrl, 1, wxEXPAND);
        auto* browseButton = new wxButton(panel, wxID_ANY, "Browse...");
        browseButton->Bind(wxEVT_BUTTON, [this, textCtrl](wxCommandEvent&) {
            BrowseForImage(textCtrl);
        });
        grid->Add(browseButton, 0);
    };

    grid->Add(new wxStaticText(panel, wxID_ANY, "Name"), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(pngTuberNameText_, 1, wxEXPAND);
    grid->AddSpacer(0);
    addPathRow("Closed Mouth / Open Eyes", closedMouthOpenEyesPathText_);
    addPathRow("Closed Mouth / Closed Eyes", closedMouthClosedEyesPathText_);
    addPathRow("Open Mouth / Open Eyes", openMouthOpenEyesPathText_);
    addPathRow("Open Mouth / Closed Eyes", openMouthClosedEyesPathText_);
    addPathRow("Mute Image (optional)", mutePathText_);

    auto* savePngTuberButton = new wxButton(panel, IdSavePngTuber, "Save PNGTuber");
    savePngTuberButton->Bind(wxEVT_BUTTON, &MainWindow::OnSavePngTuber, this);

    auto* helperText = new wxStaticText(
        panel,
        wxID_ANY,
        "Each PNGTuber stores 4 required state images plus an optional mute image."
    );

    editorBox->Add(helperText, 0, wxEXPAND | wxALL, 8);
    editorBox->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    editorBox->Add(savePngTuberButton, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    content->Add(listBox, 0, wxEXPAND | wxALL, 8);
    content->Add(editorBox, 1, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 8);

    outer->Add(headerBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    outer->Add(content, 1, wxEXPAND);
    panel->SetSizer(outer);
    notebook->AddPage(panel, "PNGTubers");
}

void MainWindow::LoadGroups()
{
    groups_.clear();
    groupChoice_->Clear();

    if (!groupStore_ || !groupStore_->IsReady()) {
        SetStatus("Groups database is unavailable.");
        return;
    }

    groups_ = groupStore_->ListGroups();
    for (const auto& group : groups_) {
        groupChoice_->Append(group.name);
    }

    if (groups_.empty()) {
        SetStatus("No groups are available.");
        return;
    }

    const auto activeGroupId = groupStore_->GetActiveGroupId();
    SelectGroupById(activeGroupId.value_or(groups_.front().id));
}

void MainWindow::LoadPngTubersForSelectedGroup()
{
    pngTubers_.clear();
    if (pngTuberList_) {
        pngTuberList_->Clear();
    }

    const auto selectedGroupId = GetSelectedGroupId();
    if (!selectedGroupId || !groupStore_ || !groupStore_->IsReady()) {
        if (pngTuberGroupLabel_) {
            pngTuberGroupLabel_->SetLabel("Active group: none");
        }
        ClearPngTuberControls();
        return;
    }

    const int selection = groupChoice_ ? groupChoice_->GetSelection() : wxNOT_FOUND;
    if (pngTuberGroupLabel_ && selection != wxNOT_FOUND && static_cast<size_t>(selection) < groups_.size()) {
        pngTuberGroupLabel_->SetLabel("Active group: " + groups_[selection].name);
    }

    pngTubers_ = groupStore_->ListPngTubersForGroup(*selectedGroupId);
    for (const auto& pngTuber : pngTubers_) {
        pngTuberList_->Append(pngTuber.name);
    }

    if (pngTubers_.empty()) {
        ClearPngTuberControls();
        return;
    }

    SelectPngTuberById(pngTubers_.front().id);
}

void MainWindow::SelectGroupById(int groupId)
{
    for (size_t index = 0; index < groups_.size(); ++index) {
        if (groups_[index].id == groupId) {
            groupChoice_->SetSelection(static_cast<int>(index));
            ApplyGroupToControls(groups_[index]);
            groupStore_->SetActiveGroup(groupId);
            LoadPngTubersForSelectedGroup();
            return;
        }
    }

    if (!groups_.empty()) {
        groupChoice_->SetSelection(0);
        ApplyGroupToControls(groups_.front());
        groupStore_->SetActiveGroup(groups_.front().id);
        LoadPngTubersForSelectedGroup();
    }
}

void MainWindow::ApplyGroupToControls(const StreamKitGroup& group)
{
    streamKitUrlText_->SetValue(group.overlayUrl);
    showBrowserWindowCheck_->SetValue(group.showBrowserWindow);
    bypassLocalNetworkPromptCheck_->SetValue(group.bypassLocalNetworkPrompt);
    pollIntervalSpin_->SetValue(group.pollIntervalMs > 0 ? group.pollIntervalMs : 250);

    std::string effectiveBrowserPath = group.browserPath;
    int matchedBrowserIndex = wxNOT_FOUND;
    for (size_t index = 0; index < browsers_.size(); ++index) {
        if (browsers_[index].path == effectiveBrowserPath) {
            matchedBrowserIndex = static_cast<int>(index);
            break;
        }
    }

    if (effectiveBrowserPath.empty() && !browsers_.empty()) {
        matchedBrowserIndex = 0;
        effectiveBrowserPath = browsers_.front().path;
    }

    browserPathText_->SetValue(effectiveBrowserPath);
    browserChoice_->SetSelection(matchedBrowserIndex);
}

void MainWindow::ApplyPngTuberToControls(const PngTuberConfig& pngTuber)
{
    pngTuberNameText_->SetValue(pngTuber.name);
    closedMouthOpenEyesPathText_->SetValue(pngTuber.closedMouthOpenEyesPath);
    closedMouthClosedEyesPathText_->SetValue(pngTuber.closedMouthClosedEyesPath);
    openMouthOpenEyesPathText_->SetValue(pngTuber.openMouthOpenEyesPath);
    openMouthClosedEyesPathText_->SetValue(pngTuber.openMouthClosedEyesPath);
    mutePathText_->SetValue(pngTuber.mutePath);
}

StreamKitGroup MainWindow::CollectGroupFromControls() const
{
    StreamKitGroup group;
    if (const auto selectedGroupId = GetSelectedGroupId()) {
        group.id = *selectedGroupId;
    }

    const int selection = groupChoice_ ? groupChoice_->GetSelection() : wxNOT_FOUND;
    if (selection != wxNOT_FOUND && static_cast<size_t>(selection) < groups_.size()) {
        group.name = groups_[selection].name;
    }

    group.overlayUrl = streamKitUrlText_->GetValue().ToStdString();
    group.browserPath = browserPathText_->GetValue().ToStdString();
    group.showBrowserWindow = showBrowserWindowCheck_->GetValue();
    group.bypassLocalNetworkPrompt = bypassLocalNetworkPromptCheck_->GetValue();
    group.pollIntervalMs = pollIntervalSpin_->GetValue();
    return group;
}

PngTuberConfig MainWindow::CollectPngTuberFromControls() const
{
    PngTuberConfig pngTuber;
    if (const auto selectedPngTuberId = GetSelectedPngTuberId()) {
        pngTuber.id = *selectedPngTuberId;
    }
    if (const auto selectedGroupId = GetSelectedGroupId()) {
        pngTuber.groupId = *selectedGroupId;
    }

    pngTuber.name = pngTuberNameText_ ? pngTuberNameText_->GetValue().ToStdString() : std::string{};
    pngTuber.closedMouthOpenEyesPath = closedMouthOpenEyesPathText_->GetValue().ToStdString();
    pngTuber.closedMouthClosedEyesPath = closedMouthClosedEyesPathText_->GetValue().ToStdString();
    pngTuber.openMouthOpenEyesPath = openMouthOpenEyesPathText_->GetValue().ToStdString();
    pngTuber.openMouthClosedEyesPath = openMouthClosedEyesPathText_->GetValue().ToStdString();
    pngTuber.mutePath = mutePathText_->GetValue().ToStdString();
    return pngTuber;
}

bool MainWindow::SaveSelectedGroupSettings(bool announceSuccess)
{
    const auto selectedGroupId = GetSelectedGroupId();
    if (!selectedGroupId) {
        SetStatus("Select a group first.");
        return false;
    }

    auto group = CollectGroupFromControls();
    group.id = *selectedGroupId;
    if (!groupStore_->UpdateGroup(group)) {
        SetStatus(groupStore_->GetLastError());
        return false;
    }

    for (auto& existingGroup : groups_) {
        if (existingGroup.id == group.id) {
            existingGroup = group;
            break;
        }
    }

    if (announceSuccess) {
        SetStatus("Saved group settings for " + group.name + ".");
    }
    return true;
}

bool MainWindow::SaveSelectedPngTuber(bool announceSuccess)
{
    const auto selectedPngTuberId = GetSelectedPngTuberId();
    if (!selectedPngTuberId) {
        SetStatus("Select a PNGTuber first.");
        return false;
    }

    auto pngTuber = CollectPngTuberFromControls();
    pngTuber.id = *selectedPngTuberId;
    pngTuber.name = wxString(pngTuber.name).Trim(true).Trim(false).ToStdString();
    if (pngTuber.name.empty()) {
        SetStatus("PNGTuber name cannot be empty.");
        return false;
    }

    if (pngTuber.closedMouthOpenEyesPath.empty() || pngTuber.closedMouthClosedEyesPath.empty() ||
        pngTuber.openMouthOpenEyesPath.empty() || pngTuber.openMouthClosedEyesPath.empty()) {
        SetStatus("All 4 mouth/eye image paths are required.");
        return false;
    }

    if (!groupStore_->UpdatePngTuber(pngTuber)) {
        SetStatus(groupStore_->GetLastError());
        return false;
    }

    for (auto& existingPngTuber : pngTubers_) {
        if (existingPngTuber.id == pngTuber.id) {
            existingPngTuber = pngTuber;
            break;
        }
    }

    const int selection = pngTuberList_ ? pngTuberList_->GetSelection() : wxNOT_FOUND;
    if (selection != wxNOT_FOUND && static_cast<size_t>(selection) < pngTubers_.size()) {
        pngTuberList_->SetString(selection, pngTuber.name);
    }

    if (announceSuccess) {
        SetStatus("Saved PNGTuber " + pngTuber.name + ".");
    }
    return true;
}

std::optional<int> MainWindow::GetSelectedGroupId() const
{
    if (!groupChoice_) {
        return std::nullopt;
    }

    const int selection = groupChoice_->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<size_t>(selection) >= groups_.size()) {
        return std::nullopt;
    }

    return groups_[selection].id;
}

std::optional<int> MainWindow::GetSelectedPngTuberId() const
{
    if (!pngTuberList_) {
        return std::nullopt;
    }

    const int selection = pngTuberList_->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<size_t>(selection) >= pngTubers_.size()) {
        return std::nullopt;
    }

    return pngTubers_[selection].id;
}

void MainWindow::SelectPngTuberById(int pngTuberId)
{
    for (size_t index = 0; index < pngTubers_.size(); ++index) {
        if (pngTubers_[index].id == pngTuberId) {
            pngTuberList_->SetSelection(static_cast<int>(index));
            ApplyPngTuberToControls(pngTubers_[index]);
            return;
        }
    }

    if (!pngTubers_.empty()) {
        pngTuberList_->SetSelection(0);
        ApplyPngTuberToControls(pngTubers_.front());
    } else {
        ClearPngTuberControls();
    }
}

void MainWindow::ClearPngTuberControls()
{
    if (pngTuberList_) {
        pngTuberList_->SetSelection(wxNOT_FOUND);
    }
    if (pngTuberNameText_) {
        pngTuberNameText_->Clear();
    }
    if (closedMouthOpenEyesPathText_) {
        closedMouthOpenEyesPathText_->Clear();
    }
    if (closedMouthClosedEyesPathText_) {
        closedMouthClosedEyesPathText_->Clear();
    }
    if (openMouthOpenEyesPathText_) {
        openMouthOpenEyesPathText_->Clear();
    }
    if (openMouthClosedEyesPathText_) {
        openMouthClosedEyesPathText_->Clear();
    }
    if (mutePathText_) {
        mutePathText_->Clear();
    }
}

void MainWindow::BrowseForImage(wxTextCtrl* target)
{
    wxFileDialog dialog(
        this,
        "Choose a PNGTuber image",
        "",
        "",
        "Image files (*.png;*.jpg;*.jpeg;*.webp;*.bmp)|*.png;*.jpg;*.jpeg;*.webp;*.bmp|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST
    );
    if (dialog.ShowModal() == wxID_OK && target) {
        target->SetValue(dialog.GetPath());
    }
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

void MainWindow::OnGroupSelected(wxCommandEvent&)
{
    const auto selectedGroupId = GetSelectedGroupId();
    if (!selectedGroupId) {
        return;
    }

    groupStore_->SetActiveGroup(*selectedGroupId);
    for (const auto& group : groups_) {
        if (group.id == *selectedGroupId) {
            ApplyGroupToControls(group);
            LoadPngTubersForSelectedGroup();
            SetStatus("Loaded group " + group.name + ".");
            break;
        }
    }
}

void MainWindow::OnCreateGroup(wxCommandEvent&)
{
    wxTextEntryDialog dialog(this, "Enter a name for the new group.", "New Group");
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString groupName = dialog.GetValue().Trim(true).Trim(false);
    if (groupName.empty()) {
        SetStatus("Group name cannot be empty.");
        return;
    }

    auto group = groupStore_->CreateGroup(groupName.ToStdString());
    if (!group) {
        SetStatus(groupStore_->GetLastError());
        return;
    }

    auto settings = CollectGroupFromControls();
    settings.id = group->id;
    settings.name = group->name;
    if (!groupStore_->UpdateGroup(settings)) {
        SetStatus(groupStore_->GetLastError());
        return;
    }

    LoadGroups();
    SelectGroupById(group->id);
    SetStatus("Created group " + group->name + ".");
}

void MainWindow::OnRenameGroup(wxCommandEvent&)
{
    const auto selectedGroupId = GetSelectedGroupId();
    if (!selectedGroupId) {
        SetStatus("Select a group first.");
        return;
    }

    const int selection = groupChoice_->GetSelection();
    wxString currentName;
    if (selection != wxNOT_FOUND) {
        currentName = groupChoice_->GetString(selection);
    }
    wxTextEntryDialog dialog(this, "Enter a new name for this group.", "Rename Group", currentName);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString groupName = dialog.GetValue().Trim(true).Trim(false);
    if (groupName.empty()) {
        SetStatus("Group name cannot be empty.");
        return;
    }

    if (!groupStore_->RenameGroup(*selectedGroupId, groupName.ToStdString())) {
        SetStatus(groupStore_->GetLastError());
        return;
    }

    LoadGroups();
    SelectGroupById(*selectedGroupId);
    SetStatus("Renamed group to " + groupName.ToStdString() + ".");
}

void MainWindow::OnDeleteGroup(wxCommandEvent&)
{
    const auto selectedGroupId = GetSelectedGroupId();
    if (!selectedGroupId) {
        SetStatus("Select a group first.");
        return;
    }

    const int selection = groupChoice_->GetSelection();
    wxString groupName = "this group";
    if (selection != wxNOT_FOUND) {
        groupName = groupChoice_->GetString(selection);
    }
    if (wxMessageBox(
            "Delete group \"" + groupName + "\"?",
            "Delete Group",
            wxYES_NO | wxICON_WARNING,
            this) != wxYES) {
        return;
    }

    if (!groupStore_->DeleteGroup(*selectedGroupId)) {
        SetStatus(groupStore_->GetLastError());
        return;
    }

    LoadGroups();
    SetStatus("Deleted group " + groupName.ToStdString() + ".");
}

void MainWindow::OnSaveGroup(wxCommandEvent&)
{
    SaveSelectedGroupSettings(true);
}

void MainWindow::OnPngTuberSelected(wxCommandEvent&)
{
    const auto selectedPngTuberId = GetSelectedPngTuberId();
    if (!selectedPngTuberId) {
        ClearPngTuberControls();
        return;
    }

    for (const auto& pngTuber : pngTubers_) {
        if (pngTuber.id == *selectedPngTuberId) {
            ApplyPngTuberToControls(pngTuber);
            SetStatus("Loaded PNGTuber " + pngTuber.name + ".");
            break;
        }
    }
}

void MainWindow::OnCreatePngTuber(wxCommandEvent&)
{
    const auto selectedGroupId = GetSelectedGroupId();
    if (!selectedGroupId) {
        SetStatus("Select a group first.");
        return;
    }

    wxTextEntryDialog dialog(this, "Enter a name for the new PNGTuber.", "New PNGTuber");
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    const wxString pngTuberName = dialog.GetValue().Trim(true).Trim(false);
    if (pngTuberName.empty()) {
        SetStatus("PNGTuber name cannot be empty.");
        return;
    }

    auto pngTuber = groupStore_->CreatePngTuber(*selectedGroupId, pngTuberName.ToStdString());
    if (!pngTuber) {
        SetStatus(groupStore_->GetLastError());
        return;
    }

    LoadPngTubersForSelectedGroup();
    SelectPngTuberById(pngTuber->id);
    SetStatus("Created PNGTuber " + pngTuber->name + ".");
}

void MainWindow::OnDeletePngTuber(wxCommandEvent&)
{
    const auto selectedPngTuberId = GetSelectedPngTuberId();
    if (!selectedPngTuberId) {
        SetStatus("Select a PNGTuber first.");
        return;
    }

    const int selection = pngTuberList_->GetSelection();
    wxString pngTuberName = "this PNGTuber";
    if (selection != wxNOT_FOUND && static_cast<size_t>(selection) < pngTubers_.size()) {
        pngTuberName = pngTubers_[selection].name;
    }

    if (wxMessageBox(
            "Delete PNGTuber \"" + pngTuberName + "\"?",
            "Delete PNGTuber",
            wxYES_NO | wxICON_WARNING,
            this) != wxYES) {
        return;
    }

    if (!groupStore_->DeletePngTuber(*selectedPngTuberId)) {
        SetStatus(groupStore_->GetLastError());
        return;
    }

    LoadPngTubersForSelectedGroup();
    SetStatus("Deleted PNGTuber " + pngTuberName.ToStdString() + ".");
}

void MainWindow::OnSavePngTuber(wxCommandEvent&)
{
    SaveSelectedPngTuber(true);
}

void MainWindow::OnMainWindowClose(wxCloseEvent& event)
{
    streamKit_->Stop();
    if (logWindow_) {
        logWindow_->Destroy();
        logWindow_ = nullptr;
        logText_ = nullptr;
    }
    event.Skip();
}

void MainWindow::OnStartStreamKit(wxCommandEvent&)
{
    SaveSelectedGroupSettings(false);
    voiceUsers_.clear();
    RenderVoiceUsers();
    streamKit_->Start(
        browserPathText_->GetValue().ToStdString(),
        streamKitUrlText_->GetValue().ToStdString(),
        showBrowserWindowCheck_->GetValue(),
        bypassLocalNetworkPromptCheck_->GetValue(),
        pollIntervalSpin_->GetValue()
    );
}

void MainWindow::OnStartStreamKitVisible(wxCommandEvent&)
{
    showBrowserWindowCheck_->SetValue(true);
    SaveSelectedGroupSettings(false);
    voiceUsers_.clear();
    RenderVoiceUsers();
    streamKit_->Start(
        browserPathText_->GetValue().ToStdString(),
        streamKitUrlText_->GetValue().ToStdString(),
        true,
        bypassLocalNetworkPromptCheck_->GetValue(),
        pollIntervalSpin_->GetValue()
    );
}

void MainWindow::OnStopStreamKit(wxCommandEvent&)
{
    streamKit_->Stop();
    SetStatus("Stopped StreamKit overlay monitor.");
}

void MainWindow::OnShowLogs(wxCommandEvent&)
{
    ShowLogsWindow();
}

void MainWindow::OnLogWindowClose(wxCloseEvent& event)
{
    if (logWindow_) {
        logWindow_->Hide();
        event.Veto();
    } else {
        event.Skip();
    }
}

void MainWindow::OnExit(wxCommandEvent&)
{
    streamKit_->Stop();
    if (logWindow_) {
        logWindow_->Destroy();
        logWindow_ = nullptr;
        logText_ = nullptr;
    }
    Close(true);
}

void MainWindow::OnAbout(wxCommandEvent&)
{
    wxMessageBox(
        "EZ PNGTuber StreamKit voice monitor prototype.",
        "About EZ PNGTuber",
        wxOK | wxICON_INFORMATION,
        this
    );
}

void MainWindow::SetStatus(const std::string& status)
{
    if (statusLabel_) {
        statusLabel_->SetLabel(status);
        statusLabel_->Wrap(880);
    }
    SetStatusText(status);
}

void MainWindow::AppendLog(const std::string& message)
{
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
    logLines_.push_back(line.str());

    constexpr size_t MaxLogLines = 1000;
    if (logLines_.size() > MaxLogLines) {
        logLines_.erase(logLines_.begin(), logLines_.begin() + static_cast<std::ptrdiff_t>(logLines_.size() - MaxLogLines));
    }

    if (logText_) {
        logText_->AppendText(line.str());
    }
}

void MainWindow::ShowLogsWindow()
{
    if (!logWindow_) {
        logWindow_ = new wxFrame(this, wxID_ANY, "EZ PNGTuber Logs", wxDefaultPosition, wxSize(900, 420));
        auto* panel = new wxPanel(logWindow_);
        auto* layout = new wxBoxSizer(wxVERTICAL);
        logText_ = new wxTextCtrl(
            panel,
            wxID_ANY,
            "",
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP
        );
        layout->Add(logText_, 1, wxEXPAND | wxALL, 8);
        panel->SetSizer(layout);
        logWindow_->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnLogWindowClose, this);
    }

    logText_->Clear();
    for (const auto& line : logLines_) {
        logText_->AppendText(line);
    }
    logWindow_->Show();
    logWindow_->Raise();
}

void MainWindow::SetVoiceUsers(std::vector<DiscordVoiceUser> users)
{
    voiceUsers_.clear();
    for (auto& user : users) {
        voiceUsers_[user.id] = std::move(user);
    }
    RenderVoiceUsers();
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

    const auto speakingCount = std::count_if(users.begin(), users.end(), [](const auto& user) {
        return user.speaking;
    });
    if (voiceUsersSummaryLabel_) {
        voiceUsersSummaryLabel_->SetLabel(wxString::Format(
            "%zu user(s) in call, %zu speaking",
            users.size(),
            static_cast<size_t>(speakingCount)
        ));
    }

    voiceUsersList_->DeleteAllItems();
    for (const auto& user : users) {
        const long row = voiceUsersList_->InsertItem(voiceUsersList_->GetItemCount(), user.displayName);
        voiceUsersList_->SetItem(row, 1, SpeakingText(user.speaking));
        voiceUsersList_->SetItem(row, 2, BoolText(user.muted));
        voiceUsersList_->SetItem(row, 3, BoolText(user.deafened));
        voiceUsersList_->SetItem(row, 4, BoolText(user.selfMuted));
        voiceUsersList_->SetItem(row, 5, BoolText(user.selfDeafened));
        voiceUsersList_->SetItem(row, 6, wxString::Format("%d", user.volume));
        if (user.speaking) {
            voiceUsersList_->SetItemBackgroundColour(row, wxColour(220, 245, 226));
        }
    }
}
