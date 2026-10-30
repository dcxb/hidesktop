#include "App.h"

#include <windows.h>

#include "DesktopIcons.h"
#include "Log.h"
#include "res/resource.h"
#include "util.h"

App::~App() {
    if (!m_shutdown) Shutdown();
    if (m_iconShown != nullptr) DestroyIcon(m_iconShown);
    if (m_iconHidden != nullptr) DestroyIcon(m_iconHidden);
}

bool App::Create(HINSTANCE instance) {
    m_instance = instance;

    m_settingsPath = Settings::DefaultPath();
    m_settings.Load(m_settingsPath);
    // Keep the log next to the settings file: portable mode then stays self contained.
    if (!m_settingsPath.empty()) {
        Log::Init(util::DirectoryOf(m_settingsPath) + L"\\hidesktop.log", m_settings.logging);
    }
    Log::Line(L"--- %s %s starting ---", kAppName, kAppVersion);

    m_iconsVisibleAtStart = DesktopIcons::IsVisible();
    if (DesktopIcons::ListView() == nullptr) {
        Log::Line(L"desktop icon list view not found at startup");
    }

    if (!RegisterWindowClass()) {
        Log::Line(L"RegisterClassEx failed: %lu", GetLastError());
        return false;
    }

    m_window = CreateWindowExW(WS_EX_TOOLWINDOW, kMainWindowClass, kAppName, WS_POPUP, 0, 0, 0, 0,
                               nullptr, nullptr, instance, this);
    if (m_window == nullptr) {
        Log::Line(L"CreateWindowEx failed: %lu", GetLastError());
        return false;
    }

    m_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    const int iconSize = GetSystemMetrics(SM_CXSMICON);
    m_iconShown = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_HIDESKTOP),
                                                IMAGE_ICON, iconSize, iconSize, LR_DEFAULTCOLOR));
    m_iconHidden = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_HIDESKTOP_HIDDEN),
                                                 IMAGE_ICON, iconSize, iconSize, LR_DEFAULTCOLOR));
    if (m_iconShown == nullptr) m_iconShown = LoadIconW(nullptr, IDI_APPLICATION);
    if (m_iconHidden == nullptr) m_iconHidden = m_iconShown;

    AddTrayIcon();
    ApplySettings(false);
    return true;
}

int App::Run() {
    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

bool App::RegisterWindowClass() {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &App::WndProcThunk;
    windowClass.hInstance = m_instance;
    windowClass.lpszClassName = kMainWindowClass;
    windowClass.hIcon = static_cast<HICON>(LoadImageW(m_instance, MAKEINTRESOURCEW(IDI_HIDESKTOP),
                                                      IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));

    if (RegisterClassExW(&windowClass) != 0) return true;
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

LRESULT CALLBACK App::WndProcThunk(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_window = hwnd;
    }

    if (self == nullptr) return DefWindowProcW(hwnd, message, wParam, lParam);
    return self->HandleMessage(message, wParam, lParam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (m_taskbarCreatedMessage != 0 && message == m_taskbarCreatedMessage) {
        Log::Line(L"taskbar recreated, restoring the tray icon");
        DesktopIcons::ResetCache();
        AddTrayIcon();
        UpdateTrayVisuals();
        return 0;
    }

    switch (message) {
        case kMsgToggleIcons:
            ToggleIcons();
            return 0;
        case kMsgHideIcons:
            SetIconsVisible(false);
            return 0;
        case kMsgShowIcons:
            SetIconsVisible(true);
            return 0;
        case kMsgShowMenu: {
            POINT cursor = {};
            GetCursorPos(&cursor);
            ShowMenu(cursor);
            return 0;
        }
        case TrayIcon::kCallbackMessage: {
            const UINT event = LOWORD(lParam);
            if (event == WM_LBUTTONUP || event == WM_LBUTTONDBLCLK) {
                ToggleIcons();
            } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
                POINT cursor = {};
                GetCursorPos(&cursor);
                ShowMenu(cursor);
            }
            return 0;
        }
        case WM_CLOSE:
            Shutdown();
            return 0;
        case WM_ENDSESSION:
            if (wParam != 0) Shutdown();
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(m_window, message, wParam, lParam);
    }
}

void App::AddTrayIcon() { UpdateTrayVisuals(); }

void App::UpdateTrayVisuals() {
    const bool visible = DesktopIcons::IsVisible();
    HICON icon = visible ? m_iconShown : m_iconHidden;

    std::wstring tooltip = L"hidesktop - 桌面图标";
    tooltip += visible ? L"：显示" : L"：隐藏";
    tooltip += m_settings.doubleClickEnabled ? L"（双击桌面切换）" : L"（双击已关闭）";

    if (!m_tray.Added()) {
        m_tray.Add(m_window, icon, tooltip);
    } else {
        m_tray.Update(icon, tooltip);
    }
}

void App::ApplySettings(bool persist) {
    InstallOrRemoveHook();
    if (persist) m_settings.Save(m_settingsPath);
    UpdateTrayVisuals();
}

void App::InstallOrRemoveHook() {
    if (!m_settings.doubleClickEnabled) {
        m_hook.Uninstall();
        return;
    }
    if (m_hook.Installed()) return;

    const bool installed = m_hook.Install(
        [this](POINT screenPoint) { return OnDesktopDoubleClick(screenPoint); });
    if (!installed) {
        m_settings.doubleClickEnabled = false;
        m_settings.Save(m_settingsPath);
        m_tray.Balloon(kAppName, L"无法安装鼠标钩子，双击切换已关闭。");
    }
}

bool App::OnDesktopDoubleClick(POINT screenPoint) {
    if (!m_settings.doubleClickEnabled) return false;

    const DesktopIcons::Probe probe = DesktopIcons::ProbePoint(screenPoint);
    if (probe != DesktopIcons::Probe::Background) return false;

    Log::Line(L"double-click on empty desktop at (%ld, %ld)", screenPoint.x, screenPoint.y);
    PostMessageW(m_window, kMsgToggleIcons, 0, 0);
    return true;
}

void App::ToggleIcons() {
    if (DesktopIcons::ListView() == nullptr) {
        Log::Line(L"toggle requested but no desktop icon list view exists");
        if (m_settings.notifications) {
            m_tray.Balloon(kAppName, L"未找到桌面图标窗口，Explorer 可能没有运行。");
        }
        return;
    }
    SetIconsVisible(!DesktopIcons::IsVisible());
}

void App::SetIconsVisible(bool visible) {
    const bool before = DesktopIcons::IsVisible();
    const bool ok = DesktopIcons::SetVisible(visible);
    const bool after = DesktopIcons::IsVisible();
    Log::Line(L"SetIconsVisible(%d): ok=%d, before=%d, after=%d", static_cast<int>(visible),
              static_cast<int>(ok), static_cast<int>(before), static_cast<int>(after));

    UpdateTrayVisuals();

    if (!m_settings.notifications) return;
    if (!ok) {
        m_tray.Balloon(kAppName, L"切换失败：无法改变桌面图标的显示状态。");
    } else if (after != before) {
        m_tray.Balloon(kAppName, after ? L"桌面图标已显示" : L"桌面图标已隐藏");
    }
}

void App::ShowMenu(POINT screenPoint) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) return;

    const bool hidden = !DesktopIcons::IsVisible();
    AppendMenuW(menu, MF_STRING, kCmdToggle, hidden ? L"显示桌面图标" : L"隐藏桌面图标");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu,
                MF_STRING | (m_settings.doubleClickEnabled ? MF_CHECKED : MF_UNCHECKED),
                kCmdDoubleClick, L"双击桌面切换图标");
    AppendMenuW(menu, MF_STRING | (Autostart::IsEnabled() ? MF_CHECKED : MF_UNCHECKED),
                kCmdAutostart, L"开机自动启动");
    AppendMenuW(menu, MF_STRING | (m_settings.notifications ? MF_CHECKED : MF_UNCHECKED),
                kCmdNotifications, L"显示操作提示");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdAbout, L"关于 hidesktop");
    AppendMenuW(menu, MF_STRING, kCmdExit, L"退出");

    SetMenuDefaultItem(menu, kCmdToggle, FALSE);

    // Required so the menu closes when the user clicks somewhere else.
    const BOOL foreground = SetForegroundWindow(m_window);
    const UINT command = static_cast<UINT>(TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, screenPoint.x, screenPoint.y, 0,
        m_window, nullptr));
    Log::Line(L"tray menu at (%ld, %ld): foreground=%d command=%u", screenPoint.x, screenPoint.y,
              static_cast<int>(foreground), command);
    DestroyMenu(menu);
    PostMessageW(m_window, WM_NULL, 0, 0);

    switch (command) {
        case kCmdToggle:
            ToggleIcons();
            break;
        case kCmdDoubleClick:
            m_settings.doubleClickEnabled = !m_settings.doubleClickEnabled;
            ApplySettings(true);
            break;
        case kCmdAutostart: {
            const bool enabled = Autostart::IsEnabled();
            if (!Autostart::SetEnabled(!enabled)) {
                m_tray.Balloon(kAppName, L"无法修改开机启动项。");
            }
            break;
        }
        case kCmdNotifications:
            m_settings.notifications = !m_settings.notifications;
            ApplySettings(true);
            break;
        case kCmdAbout:
            ShowAbout();
            break;
        case kCmdExit:
            Shutdown();
            break;
        default:
            break;
    }
}

void App::ShowAbout() {
    const std::wstring text = util::Format(
        L"hidesktop %s\n\n"
        L"双击桌面空白处：隐藏 / 显示桌面图标。\n"
        L"左键单击托盘图标：切换；右键：菜单。\n\n"
        L"命令行：\n"
        L"  hidesktop --toggle | --hide | --show | --status | --selftest\n\n"
        L"配置文件：\n%s",
        kAppVersion, m_settingsPath.c_str());
    MessageBoxW(m_window, text.c_str(), L"关于 hidesktop", MB_OK | MB_ICONINFORMATION);
}

void App::Shutdown() {
    if (m_shutdown) return;
    m_shutdown = true;

    Log::Line(L"shutting down");
    m_hook.Uninstall();

    if (m_settings.restoreOnExit) {
        DesktopIcons::SetVisible(m_iconsVisibleAtStart);
    }

    m_tray.Remove();
    if (m_window != nullptr) {
        DestroyWindow(m_window);
        m_window = nullptr;
    }
}
