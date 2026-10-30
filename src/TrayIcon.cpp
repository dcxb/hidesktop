#include "TrayIcon.h"

#include <cwchar>

bool TrayIcon::Add(HWND owner, HICON icon, const std::wstring& tooltip) {
    Remove();

    m_data = {};
    m_data.cbSize = sizeof(m_data);
    m_data.hWnd = owner;
    m_data.uID = kIconId;
    m_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_data.uCallbackMessage = kCallbackMessage;
    m_data.hIcon = icon;
    wcsncpy_s(m_data.szTip, tooltip.c_str(), _TRUNCATE);

    m_added = Shell_NotifyIconW(NIM_ADD, &m_data) != FALSE;
    return m_added;
}

void TrayIcon::Update(HICON icon, const std::wstring& tooltip) {
    if (!m_added) return;

    m_data.uFlags = NIF_ICON | NIF_TIP;
    m_data.hIcon = icon;
    wcsncpy_s(m_data.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_data);
}

void TrayIcon::Balloon(const std::wstring& title, const std::wstring& text) {
    if (!m_added) return;

    m_data.uFlags = NIF_INFO;
    m_data.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    wcsncpy_s(m_data.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(m_data.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_data);
}

void TrayIcon::Remove() {
    if (!m_added) return;
    Shell_NotifyIconW(NIM_DELETE, &m_data);
    m_added = false;
}
