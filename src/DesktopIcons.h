#pragma once

#include <windows.h>

// Access to the shell window that owns the desktop icons: the
// SHELLDLL_DefView -> SysListView32 pair that lives under Progman (or under a
// WorkerW on some configurations).
namespace DesktopIcons {

enum class Probe {
    Background,  // empty wallpaper / desktop background
    Icon,        // an actual desktop icon (label or bitmap) is under the point
    NotDesktop,  // some other window is under the point
};

// Handle of the desktop icon list view, or nullptr when the shell has no such
// window (for example while Explorer is restarting). The result is cached.
HWND ListView();

// Drops the cached handle; the next call re-resolves it.
void ResetCache();

bool IsVisible();

// Idempotent. Shows or hides the icons. Returns true when the state was
// applied (or already had the wanted value).
bool SetVisible(bool visible);

// Flips the current state; true on success.
bool Toggle();

// Classifies a screen point for double-click handling.
Probe ProbePoint(POINT screenPoint);

}  // namespace DesktopIcons
