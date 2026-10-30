#include "DoubleClickHook.h"

#include <cstdlib>

#include "Log.h"

DoubleClickHook* DoubleClickHook::s_active = nullptr;

DoubleClickHook::~DoubleClickHook() { Uninstall(); }

bool DoubleClickHook::Install(Handler handler) {
    if (m_hook != nullptr) return true;

    m_handler = std::move(handler);
    s_active = this;
    m_hook = SetWindowsHookExW(WH_MOUSE_LL, HookProc, GetModuleHandleW(nullptr), 0);
    if (m_hook == nullptr) {
        s_active = nullptr;
        m_handler = nullptr;
        Log::Line(L"SetWindowsHookEx(WH_MOUSE_LL) failed: %lu", GetLastError());
        return false;
    }
    Log::Line(L"mouse hook installed");
    return true;
}

void DoubleClickHook::Uninstall() {
    if (m_hook != nullptr) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
        Log::Line(L"mouse hook removed");
    }
    if (s_active == this) s_active = nullptr;
    m_handler = nullptr;
    m_haveLastClick = false;
    m_swallowNextRelease = false;
}

LRESULT CALLBACK DoubleClickHook::HookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && s_active != nullptr) {
        const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if (info != nullptr && s_active->OnMouseMessage(wParam, *info) != 0) return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LONG_PTR DoubleClickHook::OnMouseMessage(WPARAM message, const MSLLHOOKSTRUCT& info) {
    if (message == WM_LBUTTONUP) {
        // Swallow the release that belongs to a double-click we consumed.
        if (m_swallowNextRelease) {
            m_swallowNextRelease = false;
            return 1;
        }
        return 0;
    }

    if (message != WM_LBUTTONDOWN) return 0;

    const ULONGLONG now = GetTickCount64();
    const bool inTime =
        m_haveLastClick && (now - m_lastClickTick) <= static_cast<ULONGLONG>(GetDoubleClickTime());
    const int maxDx = GetSystemMetrics(SM_CXDOUBLECLK) / 2;
    const int maxDy = GetSystemMetrics(SM_CYDOUBLECLK) / 2;
    const bool inSpace = std::abs(info.pt.x - m_lastClickPoint.x) <= maxDx &&
                         std::abs(info.pt.y - m_lastClickPoint.y) <= maxDy;
    const bool isDoubleClick = inTime && inSpace;

    if (isDoubleClick && m_handler) {
        // A handled double-click terminates the sequence.
        m_haveLastClick = false;
        if (m_handler(info.pt)) {
            m_swallowNextRelease = true;
            Log::Line(L"double-click consumed at (%ld, %ld)", info.pt.x, info.pt.y);
            return 1;
        }
    }

    m_haveLastClick = true;
    m_lastClickTick = static_cast<DWORD>(now);
    m_lastClickPoint = info.pt;
    return 0;
}
