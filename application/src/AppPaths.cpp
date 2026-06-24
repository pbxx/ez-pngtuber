#include "AppPaths.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace {
std::string EnsureAppDirectory(const wxString& childPath = {})
{
    wxFileName path(wxStandardPaths::Get().GetUserLocalDataDir(), "");
    if (!childPath.empty()) {
        path.AppendDir(childPath);
    }

    const wxString fullPath = path.GetPath();
    wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return fullPath.ToStdString();
}
}

std::string GetAppDataDirectory()
{
    return EnsureAppDirectory();
}

std::string GetGroupDatabasePath()
{
    wxFileName databasePath(wxString::FromUTF8(GetAppDataDirectory()), "groups.db");
    return databasePath.GetFullPath().ToStdString();
}

std::string GetStreamKitProfilePath()
{
    wxFileName profilePath(wxString::FromUTF8(GetAppDataDirectory()), "");
    profilePath.AppendDir("StreamKitBrowserProfile");
    wxFileName::Mkdir(profilePath.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return profilePath.GetPath().ToStdString();
}
