#include "DesktopIcons.h"

#include <commctrl.h>

#include <cwchar>

#include "Log.h"

namespace DesktopIcons {
namespace {

constexpr wchar_t kDefViewClass[] = L"SHELLDLL_DefView";
constexpr wchar_t kListViewClass[] = L"SysListView32";
constexpr wchar_t kProgmanClass[] = L"Progman";
constexpr wchar_t kWorkerWClass[] = L"WorkerW";

// Explorer's "View -> Show desktop icons" command id. Used as a fallback when
// ShowWindow() alone does not change the visibility of the list view.
constexpr WPARAM kCmdShowDesktopIcons = 0x7402;

HWND g_listView = nullptr;

bool ClassIs(HWND hwnd, const wchar_t* name) {
    if (hwnd == nullptr) return false;
    wchar_t buffer[64] = {};
    if (GetClassNameW(hwnd, buffer, _countof(buffer)) == 0) return false;
    return _wcsicmp(buffer, name) == 0;
}

struct ClassSearch {
    const wchar_t* className = nullptr;
    HWND found = nullptr;
};

BOOL CALLBACK ClassSearchProc(HWND hwnd, LPARAM param) {
    ClassSearch* search = reinterpret_cast<ClassSearch*>(param);
    if (ClassIs(hwnd, search->className)) {
        search->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND FindChildByClass(HWND parent, const wchar_t* className) {
    if (parent == nullptr) return nullptr;
    HWND direct = FindWindowExW(parent, nullptr, className, nullptr);
    if (direct != nullptr) return direct;

    ClassSearch search{className, nullptr};
    EnumChildWindows(parent, ClassSearchProc, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

bool IsDesktopDefView(HWND defView) {
    if (!ClassIs(defView, kDefViewClass)) return false;
    HWND host = GetParent(defView);
    return ClassIs(host, kProgmanClass) || ClassIs(host, kWorkerWClass);
}

HWND ResolveListView() {
    // The common layout: Progman -> SHELLDLL_DefView -> SysListView32.
    HWND progman = FindWindowW(kProgmanClass, nullptr);
    if (progman != nullptr) {
        HWND defView = FindWindowExW(progman, nullptr, kDefViewClass, nullptr);
        if (HWND listView = FindChildByClass(defView, kListViewClass)) return listView;
    }

    // Wallpaper slideshow and some Windows 11 builds host the icons under WorkerW.
    for (HWND worker = nullptr;
         (worker = FindWindowExW(nullptr, worker, kWorkerWClass, nullptr)) != nullptr;) {
        HWND defView = FindWindowExW(worker, nullptr, kDefViewClass, nullptr);
        if (HWND listView = FindChildByClass(defView, kListViewClass)) return listView;
    }

    // Last resort: search every top level window for a desktop DefView.
    for (HWND top = nullptr; (top = FindWindowExW(nullptr, top, nullptr, nullptr)) != nullptr;) {
        HWND defView = FindChildByClass(top, kDefViewClass);
        if (!IsDesktopDefView(defView)) continue;
        if (HWND listView = FindChildByClass(defView, kListViewClass)) return listView;
    }

    return nullptr;
}

enum class ItemHit { Item, Empty, Unknown };

// Asks the list view whether an icon is under the point. LVM_HITTEST takes a
// pointer, so the LVHITTESTINFO has to live inside the shell process.
ItemHit HitTestItem(HWND listView, POINT screenPoint) {
    POINT client = screenPoint;
    if (!ScreenToClient(listView, &client)) return ItemHit::Unknown;

    DWORD processId = 0;
    GetWindowThreadProcessId(listView, &processId);
    if (processId == 0) return ItemHit::Unknown;

    HANDLE process =
        OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (process == nullptr) return ItemHit::Unknown;

    ItemHit hit = ItemHit::Unknown;
    void* remote =
        VirtualAllocEx(process, nullptr, sizeof(LVHITTESTINFO), MEM_COMMIT | MEM_RESERVE,
                       PAGE_READWRITE);
    if (remote != nullptr) {
        LVHITTESTINFO info = {};
        info.pt = client;

        SIZE_T transferred = 0;
        if (WriteProcessMemory(process, remote, &info, sizeof(info), &transferred) &&
            transferred == sizeof(info)) {
            DWORD_PTR result = 0;
            const LRESULT sent =
                SendMessageTimeoutW(listView, LVM_HITTEST, 0,
                                    reinterpret_cast<LPARAM>(remote),
                                    SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &result);
            if (sent != 0 && ReadProcessMemory(process, remote, &info, sizeof(info), &transferred) &&
                transferred == sizeof(info)) {
                hit = info.iItem >= 0 ? ItemHit::Item : ItemHit::Empty;
            }
        }
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    }

    CloseHandle(process);
    return hit;
}

}  // namespace

HWND ListView() {
    if (g_listView != nullptr && IsWindow(g_listView) &&
        ClassIs(g_listView, kListViewClass)) {
        return g_listView;
    }
    g_listView = ResolveListView();
    if (g_listView != nullptr) {
        Log::Line(L"desktop icon list view resolved to 0x%p", static_cast<void*>(g_listView));
    }
    return g_listView;
}

void ResetCache() {
    Log::Line(L"desktop icon list view cache cleared");
    g_listView = nullptr;
}

bool IsVisible() {
    HWND listView = ListView();
    return listView != nullptr && IsWindowVisible(listView) != FALSE;
}

bool SetVisible(bool visible) {
    HWND listView = ListView();
    if (listView == nullptr) return false;

    if ((IsWindowVisible(listView) != FALSE) == visible) return true;

    ShowWindow(listView, visible ? SW_SHOWNA : SW_HIDE);
    if ((IsWindowVisible(listView) != FALSE) == visible) return true;

    // Some shell versions ignore ShowWindow on the list view; ask Explorer to
    // run its own toggle command instead.
    Log::Line(L"ShowWindow failed, falling back to the shell toggle command");
    HWND defView = GetParent(listView);
    if (defView != nullptr) {
        DWORD_PTR result = 0;
        SendMessageTimeoutW(defView, WM_COMMAND, kCmdShowDesktopIcons, 0,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 250, &result);
    }
    return (IsWindowVisible(listView) != FALSE) == visible;
}

bool Toggle() {
    HWND listView = ListView();
    if (listView == nullptr) return false;
    return SetVisible(IsWindowVisible(listView) == FALSE);
}

Probe ProbePoint(POINT screenPoint) {
    HWND hwnd = WindowFromPoint(screenPoint);
    if (hwnd == nullptr) return Probe::NotDesktop;

    if (ClassIs(hwnd, kListViewClass)) {
        // A folder window also uses SysListView32; only the desktop DefView counts.
        if (!IsDesktopDefView(GetParent(hwnd))) return Probe::NotDesktop;
        switch (HitTestItem(hwnd, screenPoint)) {
            case ItemHit::Item:
                return Probe::Icon;
            case ItemHit::Empty:
                return Probe::Background;
            case ItemHit::Unknown:
            default:
                // Fail safe: never take over a click we cannot classify.
                return Probe::Icon;
        }
    }

    // With the icons hidden the wallpaper itself is the window under the cursor.
    if (ClassIs(hwnd, kProgmanClass) || ClassIs(hwnd, kWorkerWClass) ||
        ClassIs(hwnd, kDefViewClass)) {
        return Probe::Background;
    }

    return Probe::NotDesktop;
}

}  // namespace DesktopIcons
