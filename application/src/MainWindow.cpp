#include "MainWindow.h"

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statusbr.h>

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, "EZ PNGTuber 0.1.0", wxDefaultPosition, wxSize(960, 640))
{
    auto* panel = new wxPanel(this);
    auto* layout = new wxBoxSizer(wxVERTICAL);

    auto* titleLabel = new wxStaticText(panel, wxID_ANY, "EZ PNGTuber");
    auto titleFont = titleLabel->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 8);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    titleLabel->SetFont(titleFont);

    statusLabel_ = new wxStaticText(
        panel,
        wxID_ANY,
        "wxWidgets is running from C++. Replace this placeholder with the PNGTuber scene.",
        wxDefaultPosition,
        wxDefaultSize,
        wxALIGN_CENTER_HORIZONTAL
    );
    statusLabel_->Wrap(720);

    auto* button = new wxButton(panel, wxID_ANY, "Click me");
    button->Bind(wxEVT_BUTTON, &MainWindow::OnButtonClicked, this);

    layout->AddStretchSpacer();
    layout->Add(titleLabel, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 16);
    layout->Add(statusLabel_, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxBOTTOM, 24);
    layout->Add(button, 0, wxALIGN_CENTER_HORIZONTAL);
    layout->AddStretchSpacer();

    panel->SetSizerAndFit(layout);

    CreateStatusBar();
    SetStatusText("Ready");
    Centre();
}

void MainWindow::OnButtonClicked(wxCommandEvent&)
{
    ++clickCount_;
    statusLabel_->SetLabel(wxString::Format("Button clicked %d time(s). The event loop is alive.", clickCount_));
    statusLabel_->Wrap(720);
    Layout();
}
