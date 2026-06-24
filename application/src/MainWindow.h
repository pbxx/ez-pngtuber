#pragma once

#include <wx/frame.h>

class wxCommandEvent;
class wxStaticText;

class MainWindow final : public wxFrame {
public:
    MainWindow();

private:
    void OnButtonClicked(wxCommandEvent& event);

    wxStaticText* statusLabel_ = nullptr;
    int clickCount_ = 0;
};
