#include "MainWindow.h"

#include <wx/app.h>

class EzPngTuberApp final : public wxApp {
public:
    bool OnInit() override;
};

bool EzPngTuberApp::OnInit()
{
    if (!wxApp::OnInit()) {
        return false;
    }

    SetAppName("EZ PNGTuber");
    SetVendorName("pbxx");

    auto* window = new MainWindow();
    window->Show(true);
    return true;
}

wxIMPLEMENT_APP(EzPngTuberApp);
