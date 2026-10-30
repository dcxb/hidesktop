#pragma once

#include <windows.h>

#include <functional>

// Watches the left mouse button with a WH_MOUSE_LL hook and reports
// double-clicks. The handler decides whether the double-click belongs to us:
// returning true swallows the second press and its release before the shell
// ever sees them, which keeps Explorer from acting on the click.
class DoubleClickHook {
public:
    using Handler = std::function<bool(POINT screenPoint)>;

    DoubleClickHook() = default;
    ~DoubleClickHook();
    DoubleClickHook(const DoubleClickHook&) = delete;
    DoubleClickHook& operator=(const DoubleClickHook&) = delete;

    bool Install(Handler handler);
    void Uninstall();
    bool Installed() const { return m_hook != nullptr; }

private:
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam);
    LONG_PTR OnMouseMessage(WPARAM message, const MSLLHOOKSTRUCT& info);

    static DoubleClickHook* s_active;

    HHOOK m_hook = nullptr;
    Handler m_handler;

    bool m_haveLastClick = false;
    DWORD m_lastClickTick = 0;
    POINT m_lastClickPoint = {};
    bool m_swallowNextRelease = false;
};
