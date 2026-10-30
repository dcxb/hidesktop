#include "Settings.h"

#include "util.h"

namespace {

constexpr wchar_t kSection[] = L"general";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"hidesktop";

bool ReadFlag(const std::wstring& path, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(kSection, key, fallback ? 1 : 0, path.c_str()) != 0;
}

void WriteFlag(const std::wstring& path, const wchar_t* key, bool value) {
    WritePrivateProfileStringW(kSection, key, value ? L"1" : L"0", path.c_str());
}

std::wstring StartupCommand() { return L"\"" + util::ExePath() + L"\" --startup"; }

bool ReadRunValue(std::wstring& value) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;

    wchar_t buffer[1024] = {};
    DWORD bytes = static_cast<DWORD>(sizeof(buffer) - sizeof(wchar_t));
    DWORD type = 0;
    const LONG status = RegQueryValueExW(key, kRunValue, nullptr, &type,
                                         reinterpret_cast<LPBYTE>(buffer), &bytes);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return false;
    value.assign(buffer, bytes / sizeof(wchar_t));
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return true;
}

}  // namespace

std::wstring Settings::DefaultPath() {
    // Portable mode: an ini next to the executable wins, so a single copy of
    // hidesktop.exe keeps its settings with it and touches nothing else.
    const std::wstring beside = util::DirectoryOf(util::ExePath()) + L"\\hidesktop.ini";
    if (GetFileAttributesW(beside.c_str()) != INVALID_FILE_ATTRIBUTES) return beside;

    const std::wstring directory = util::AppDataDirectory();
    if (directory.empty()) return std::wstring();
    return directory + L"\\hidesktop.ini";
}

void Settings::Load(const std::wstring& path) {
    if (path.empty()) return;
    doubleClickEnabled = ReadFlag(path, L"doubleClick", doubleClickEnabled);
    notifications = ReadFlag(path, L"notifications", notifications);
    restoreOnExit = ReadFlag(path, L"restoreOnExit", restoreOnExit);
    logging = ReadFlag(path, L"logging", logging);
}

void Settings::Save(const std::wstring& path) const {
    if (path.empty()) return;
    // Only create %APPDATA%\hidesktop once there is something to persist.
    util::EnsureDirectory(util::DirectoryOf(path));
    WriteFlag(path, L"doubleClick", doubleClickEnabled);
    WriteFlag(path, L"notifications", notifications);
    WriteFlag(path, L"restoreOnExit", restoreOnExit);
    WriteFlag(path, L"logging", logging);
}

namespace Autostart {

bool IsEnabled() {
    std::wstring value;
    return ReadRunValue(value);
}

bool SetEnabled(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_QUERY_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    bool ok = true;
    if (enabled) {
        const std::wstring command = StartupCommand();
        ok = RegSetValueExW(key, kRunValue, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(command.c_str()),
                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) ==
             ERROR_SUCCESS;
    } else {
        const LONG status = RegDeleteValueW(key, kRunValue);
        ok = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(key);
    return ok;
}

}  // namespace Autostart
