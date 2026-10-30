#pragma once

#include <windows.h>

#include <string>

#include "DoubleClickHook.h"
#include "Settings.h"
#include "TrayIcon.h"

constexpr wchar_t kAppName[] = L"hidesktop";
constexpr wchar_t kAppVersion[] = L"1.0.0";
constexpr wchar_t kMainWindowClass[] = L"hidesktop.main-window";

// Commands another instance (or the CLI) posts to the running app.
constexpr UINT kMsgToggleIcons = WM_APP + 1;
constexpr UINT kMsgHideIcons = WM_APP + 2;
constexpr UINT kMsgShowIcons = WM_APP + 3;
constexpr UINT kMsgShowMenu = WM_APP + 4;

// Owns the hidden window, the tray icon and the mouse hook.
class App {
public:
    App() = default;
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool Create(HINSTANCE instance);
    int Run();

    HWND Window() const { return m_window; }
    const Settings& CurrentSettings() const { return m_settings; }

private:
    enum MenuCommand : UINT {
        kCmdToggle = 1001,
        kCmdDoubleClick,
        kCmdAutostart,
        kCmdNotifications,
        kCmdAbout,
        kCmdExit,
    };

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass();
    void AddTrayIcon();
    void UpdateTrayVisuals();
    void ApplySettings(bool persist);
    void InstallOrRemoveHook();

    void ToggleIcons();
    void SetIconsVisible(bool visible);
    bool OnDesktopDoubleClick(POINT screenPoint);

    void ShowMenu(POINT screenPoint);
    void ShowAbout();
    void Shutdown();

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;

    Settings m_settings;
    std::wstring m_settingsPath;

    TrayIcon m_tray;
    DoubleClickHook m_hook;
    HICON m_iconShown = nullptr;
    HICON m_iconHidden = nullptr;

    UINT m_taskbarCreatedMessage = 0;
    bool m_iconsVisibleAtStart = true;
    bool m_shutdown = false;
};
