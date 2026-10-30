#pragma once

#include <windows.h>

#include <string>

// Persisted in %APPDATA%\hidesktop\hidesktop.ini.
struct Settings {
    bool doubleClickEnabled = true;  // double-click on the desktop toggles the icons
    bool notifications = true;       // balloon notifications on toggle
    bool restoreOnExit = true;       // restore the icon state seen at startup
    bool logging = false;

    static std::wstring DefaultPath();

    void Load(const std::wstring& path);
    void Save(const std::wstring& path) const;
};

// HKCU\...\CurrentVersion\Run integration.
namespace Autostart {

bool IsEnabled();
bool SetEnabled(bool enabled);

}  // namespace Autostart
