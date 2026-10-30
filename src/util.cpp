#include "util.h"

#include <shlobj.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace util {
namespace {

std::wstring StripTrailingSeparators(std::wstring path) {
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    return path;
}

HANDLE StandardHandle(DWORD id) {
    HANDLE handle = GetStdHandle(id);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return nullptr;
    return handle;
}

void WriteText(DWORD standardId, const std::wstring& text) {
    HANDLE handle = StandardHandle(standardId);
    if (!handle) return;

    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode)) {
        DWORD written = 0;
        WriteConsoleW(handle, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }

    // Redirected to a file or pipe: speak UTF-8.
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) return;
    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), length,
                        nullptr, nullptr);
    DWORD written = 0;
    WriteFile(handle, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

}  // namespace

std::wstring ExePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return std::wstring();
        if (length < buffer.size()) return std::wstring(buffer.data(), length);
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring DirectoryOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return std::wstring();
    return StripTrailingSeparators(path.substr(0, slash));
}

std::wstring AppDataDirectory() {
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer,
                                                static_cast<DWORD>(_countof(buffer)));
    std::wstring path;
    if (length > 0 && length < static_cast<DWORD>(_countof(buffer))) {
        path.assign(buffer, length);
    } else {
        path = DirectoryOf(ExePath());
    }
    if (path.empty()) return std::wstring();
    return StripTrailingSeparators(path) + L"\\hidesktop";
}

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) return false;
    const int status = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return status == ERROR_SUCCESS || status == ERROR_ALREADY_EXISTS;
}

std::wstring Format(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    std::wstring text = FormatV(format, args);
    va_end(args);
    return text;
}

std::wstring FormatV(const wchar_t* format, va_list args) {
    va_list probe;
    va_copy(probe, args);
    const int needed = _vscwprintf(format, probe);
    va_end(probe);
    if (needed <= 0) return std::wstring();

    std::wstring text(static_cast<size_t>(needed), L'\0');
    _vsnwprintf_s(text.data(), text.size() + 1, _TRUNCATE, format, args);
    return text;
}

bool AttachParentConsole() {
    if (GetConsoleWindow() != nullptr) return true;

    // A redirected stdout/stderr (inherited from the launcher) must be kept:
    // AttachConsole would replace it with console handles.
    HANDLE standard = GetStdHandle(STD_OUTPUT_HANDLE);
    if (standard != nullptr && standard != INVALID_HANDLE_VALUE) return true;

    if (AttachConsole(ATTACH_PARENT_PROCESS)) return true;
    return GetLastError() == ERROR_ACCESS_DENIED;  // already attached
}

void WriteStdOut(const std::wstring& text) { WriteText(STD_OUTPUT_HANDLE, text); }

void WriteStdErr(const std::wstring& text) { WriteText(STD_ERROR_HANDLE, text); }

}  // namespace util
