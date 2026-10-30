#pragma once

#include <windows.h>

#include <string>

namespace util {

// Absolute path of the running executable.
std::wstring ExePath();

// Directory part of a path, without a trailing separator.
std::wstring DirectoryOf(const std::wstring& path);

// %APPDATA%\hidesktop (not created here; the settings writer creates it on demand).
std::wstring AppDataDirectory();

// Creates a directory and every missing parent. True when it exists afterwards.
bool EnsureDirectory(const std::wstring& path);

std::wstring Format(const wchar_t* format, ...);
std::wstring FormatV(const wchar_t* format, va_list args);

// Console access for a GUI subsystem binary.
bool AttachParentConsole();
void WriteStdOut(const std::wstring& text);
void WriteStdErr(const std::wstring& text);

}  // namespace util
