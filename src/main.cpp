#include <windows.h>
#include <shellapi.h>

#include <cwctype>
#include <string>
#include <vector>

#include "App.h"
#include "DesktopIcons.h"
#include "Settings.h"
#include "util.h"

namespace {

constexpr wchar_t kMutexName[] = L"Local\\hidesktop-singleton-{6C1F4A2E-7B3D-4E58-9A11-2F0C7D5E8B43}";

std::wstring Lower(std::wstring text) {
    for (wchar_t& character : text) character = static_cast<wchar_t>(towlower(character));
    return text;
}

HWND FindRunningInstance() { return FindWindowW(kMainWindowClass, nullptr); }

const wchar_t* ProbeName(DesktopIcons::Probe probe) {
    switch (probe) {
        case DesktopIcons::Probe::Background:
            return L"desktop-background";
        case DesktopIcons::Probe::Icon:
            return L"desktop-icon";
        case DesktopIcons::Probe::NotDesktop:
        default:
            return L"other-window";
    }
}

void ShowHelp() {
    util::AttachParentConsole();
    util::WriteStdOut(
        L"hidesktop " + std::wstring(kAppVersion) +
        L"\n\n"
        L"用法:\n"
        L"  hidesktop                 启动托盘程序（已在运行时切换到该实例）\n"
        L"  hidesktop --startup       开机自启动入口，行为同上\n"
        L"  hidesktop --toggle        隐藏/显示桌面图标一次，然后退出\n"
        L"  hidesktop --hide          隐藏桌面图标\n"
        L"  hidesktop --show          显示桌面图标\n"
        L"  hidesktop --status        打印 visible/hidden（退出码 0=显示 1=隐藏）\n"
        L"  hidesktop --probe [x y]   诊断：判断某屏幕坐标是否属于桌面空白处\n"
        L"  hidesktop --selftest      自检：查找桌面窗口并往返切换一次\n"
        L"  hidesktop --help          显示本帮助\n\n"
        L"运行后双击桌面空白处即可隐藏/显示桌面图标。\n");
}

int PrintStatus() {
    util::AttachParentConsole();
    HWND listView = DesktopIcons::ListView();
    if (listView == nullptr) {
        util::WriteStdErr(L"status: 未找到桌面图标窗口\n");
        return 2;
    }
    const bool visible = IsWindowVisible(listView) != FALSE;
    util::WriteStdOut(visible ? L"visible\n" : L"hidden\n");
    return visible ? 0 : 1;
}

int RunSelfTest() {
    util::AttachParentConsole();

    std::wstring report = util::Format(L"hidesktop %s self test\n", kAppVersion);
    report += util::Format(L"  executable  : %s\n", util::ExePath().c_str());
    report += util::Format(L"  settings    : %s\n", Settings::DefaultPath().c_str());

    HWND listView = DesktopIcons::ListView();
    if (listView == nullptr) {
        report += L"  list view   : NOT FOUND\n";
        report += L"RESULT: FAIL\n";
        util::WriteStdErr(report);
        return 2;
    }

    wchar_t className[64] = {};
    GetClassNameW(listView, className, _countof(className));
    const bool original = IsWindowVisible(listView) != FALSE;
    report += util::Format(L"  list view   : 0x%p (%s), visible=%d\n",
                           reinterpret_cast<void*>(listView), className, static_cast<int>(original));

    const bool hideOk = DesktopIcons::SetVisible(false) && !DesktopIcons::IsVisible();
    report += util::Format(L"  hide        : %s\n", hideOk ? L"OK" : L"FAILED");

    const bool showOk = DesktopIcons::SetVisible(true) && DesktopIcons::IsVisible();
    report += util::Format(L"  show        : %s\n", showOk ? L"OK" : L"FAILED");

    const bool restoreOk = DesktopIcons::SetVisible(original) &&
                           (DesktopIcons::IsVisible() == original);
    report += util::Format(L"  restore     : %s\n", restoreOk ? L"OK" : L"FAILED");

    POINT cursor = {};
    if (GetCursorPos(&cursor)) {
        report += util::Format(L"  probe(cursor): %s at (%ld, %ld)\n",
                               ProbeName(DesktopIcons::ProbePoint(cursor)), cursor.x, cursor.y);
    }

    const bool ok = hideOk && showOk && restoreOk;
    report += ok ? L"RESULT: PASS\n" : L"RESULT: FAIL\n";
    util::WriteStdOut(report);
    return ok ? 0 : 1;
}

int RunOneShot(const std::wstring& command) {
    // If the tray app runs, let it own the state so its icon and tooltip stay in sync.
    if (HWND running = FindRunningInstance()) {
        const UINT message = command == L"--toggle" ? kMsgToggleIcons
                             : command == L"--hide" ? kMsgHideIcons
                                                    : kMsgShowIcons;
        PostMessageW(running, message, 0, 0);
        return 0;
    }

    const bool ok = command == L"--toggle" ? DesktopIcons::Toggle()
                                           : DesktopIcons::SetVisible(command == L"--show");
    if (!ok) {
        util::AttachParentConsole();
        util::WriteStdErr(L"hidesktop: 操作失败（未找到桌面图标窗口）\n");
        return 1;
    }
    return 0;
}

int PrintProbe(const std::vector<std::wstring>& arguments) {
    util::AttachParentConsole();

    POINT point = {};
    if (arguments.size() >= 3) {
        point.x = _wtoi(arguments[1].c_str());
        point.y = _wtoi(arguments[2].c_str());
    } else if (!GetCursorPos(&point)) {
        util::WriteStdErr(L"probe: 无法读取鼠标位置\n");
        return 2;
    }

    HWND hwnd = WindowFromPoint(point);
    wchar_t className[64] = {};
    if (hwnd != nullptr) GetClassNameW(hwnd, className, _countof(className));

    util::WriteStdOut(util::Format(L"%s (%ld, %ld) window=0x%p class=%s\n",
                                   ProbeName(DesktopIcons::ProbePoint(point)), point.x, point.y,
                                   reinterpret_cast<void*>(hwnd), className));
    return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    std::vector<std::wstring> arguments;
    int argumentCount = 0;
    if (LPWSTR* raw = CommandLineToArgvW(GetCommandLineW(), &argumentCount)) {
        for (int index = 1; index < argumentCount; ++index) arguments.emplace_back(raw[index]);
        LocalFree(raw);
    }

    std::wstring command;
    if (!arguments.empty()) command = Lower(arguments[0]);

    if (command == L"--help" || command == L"-h" || command == L"-?" || command == L"/?") {
        ShowHelp();
        return 0;
    }
    if (command == L"--status") return PrintStatus();
    if (command == L"--selftest") return RunSelfTest();
    if (command == L"--probe") return PrintProbe(arguments);
    if (command == L"--toggle" || command == L"--hide" || command == L"--show") {
        return RunOneShot(command);
    }

    // Normal (or --startup) launch: a single tray instance per user session.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    const bool alreadyRunning = mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS;

    if (alreadyRunning) {
        if (HWND running = FindRunningInstance()) {
            PostMessageW(running, kMsgShowMenu, 0, 0);
        }
        if (mutex != nullptr) CloseHandle(mutex);
        return 0;
    }

    App app;
    if (!app.Create(instance)) {
        MessageBoxW(nullptr, L"hidesktop 启动失败。", kAppName, MB_ICONERROR | MB_OK);
        if (mutex != nullptr) CloseHandle(mutex);
        return 1;
    }

    const int result = app.Run();
    if (mutex != nullptr) CloseHandle(mutex);
    return result;
}
