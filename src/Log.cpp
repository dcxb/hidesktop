#include "Log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace Log {
namespace {

bool g_enabled = false;
std::wstring g_path;

}  // namespace

void Init(const std::wstring& path, bool enabled) {
    g_path = path;
    g_enabled = enabled;
}

bool Enabled() { return g_enabled; }

void Line(const wchar_t* format, ...) {
    if (!g_enabled || g_path.empty()) return;

    SYSTEMTIME now = {};
    GetLocalTime(&now);

    wchar_t stamp[40] = {};
    swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u.%03u ", now.wYear, now.wMonth, now.wDay,
               now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);

    wchar_t message[1024] = {};
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(message, _TRUNCATE, format, args);
    va_end(args);

    FILE* file = nullptr;
    if (_wfopen_s(&file, g_path.c_str(), L"a, ccs=UTF-8") != 0 || file == nullptr) return;
    fputws(stamp, file);
    fputws(message, file);
    fputws(L"\n", file);
    fclose(file);
}

}  // namespace Log
