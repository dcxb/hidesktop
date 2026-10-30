#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

// Thin wrapper around Shell_NotifyIcon (classic, non version-4 behaviour: the
// callback message receives the raw mouse message in LPARAM).
class TrayIcon {
public:
    static constexpr UINT kCallbackMessage = WM_APP + 100;
    static constexpr UINT kIconId = 1;

    TrayIcon() = default;
    ~TrayIcon() { Remove(); }
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    bool Add(HWND owner, HICON icon, const std::wstring& tooltip);
    void Update(HICON icon, const std::wstring& tooltip);
    void Balloon(const std::wstring& title, const std::wstring& text);
    void Remove();
    bool Added() const { return m_added; }

private:
    NOTIFYICONDATAW m_data = {};
    bool m_added = false;
};
