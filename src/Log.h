#pragma once

#include <string>

namespace Log {

// Enables or disables logging. Existing file content is appended to.
void Init(const std::wstring& path, bool enabled);

void Line(const wchar_t* format, ...);

bool Enabled();

}  // namespace Log
