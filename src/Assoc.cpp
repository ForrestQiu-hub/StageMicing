// ---------------------------------------------------------------------------
// .smproj FILE ASSOCIATION (item 5)
//
// Everything is written under HKCU — no elevation, no installer, and the
// 「解除文件关联」 entry can remove exactly what 「注册」 created:
//
//   HKCU\Software\Classes\.smproj              (Default) = "StageMicing.Project"
//                                             Content Type = "application/x-smproj"
//   HKCU\Software\Classes\StageMicing.Project  (Default) = "Stage Micing Project"
//                                             DefaultIcon = "<exe>",0
//                                             shell\open\command = "\"<exe>\" \"%1\""
//
// The same shape is shipped as tools\associate-smproj.reg (UTF-16LE, REGEDIT5)
// for machines where the app itself may not be launched first.
// ---------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "App.h"
#include "L10n.h"

#include <string>

namespace {

constexpr const wchar_t* kProgId = L"StageMicing.Project";

bool SetStr(HKEY root, const std::wstring& sub, const wchar_t* name,
            const std::wstring& value) {
    HKEY k = nullptr;
    if (RegCreateKeyExW(root, sub.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k,
                        nullptr) != ERROR_SUCCESS)
        return false;
    const DWORD bytes = (DWORD)((value.size() + 1) * sizeof(wchar_t));
    const LONG rc = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)value.c_str(),
                                   bytes);
    RegCloseKey(k);
    return rc == ERROR_SUCCESS;
}

bool SetSz(HKEY root, const std::wstring& sub, const wchar_t* name,
           const wchar_t* value) {
    return SetStr(root, sub, name, value);
}

void DelTree(HKEY root, const std::wstring& sub) {
    // RegDeleteTreeW removes the key and every child; a missing key is fine
    RegDeleteTreeW(root, sub.c_str());
}

} // namespace

bool App::RegisterSmproj() {
    const std::wstring exe = exeDir_ + L"\\StageMicing.exe";
    const std::wstring cls = std::wstring(L"Software\\Classes\\") + kProgId;
    bool ok = true;
    struct Pair {
        std::wstring sub;
        const wchar_t* name;
        std::wstring value;
    };
    const std::wstring dot = L"Software\\Classes\\.smproj";
    ok &= SetSz(HKEY_CURRENT_USER, dot, nullptr, kProgId);
    ok &= SetSz(HKEY_CURRENT_USER, dot, L"Content Type", L"application/x-smproj");
    ok &= SetSz(HKEY_CURRENT_USER, cls, nullptr, L"Stage Micing Project");
    // the icon is a SUBKEY with a default value, not a named value of the class
    ok &= SetStr(HKEY_CURRENT_USER, cls + L"\\DefaultIcon", nullptr, exe + L",0");
    // the command line is quoted twice: the exe path and the "%1" placeholder
    const std::wstring cmd = L"\"" + exe + L"\" \"%1\"";
    ok &= SetStr(HKEY_CURRENT_USER, cls + L"\\shell\\open\\command", nullptr, cmd);
    if (!ok) {
        StatusMsg(L10n::T(S::AssocFailed), true);
        return false;
    }
    StatusMsg(L10n::T(S::AssocDone));
    InvalidateAll();
    return true;
}

bool App::UnregisterSmproj() {
    DelTree(HKEY_CURRENT_USER, std::wstring(L"Software\\Classes\\") + kProgId);
    DelTree(HKEY_CURRENT_USER, L"Software\\Classes\\.smproj");
    StatusMsg(L10n::T(S::AssocRemoved));
    InvalidateAll();
    return true;
}
