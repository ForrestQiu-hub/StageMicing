#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include "App.h"
#include "DebugLog.h"

#pragma comment(lib, "comctl32.lib")

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // Per-monitor DPI awareness so the plan stays crisp on scaled displays.
    using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        auto setCtx = reinterpret_cast<SetCtxFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setCtx) setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    DLog("main: dpi ok");
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    DLog("main: comctl ok");
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &gsi, nullptr) != Gdiplus::Ok) return 1;

    DLog("main: gdi+ ok");
    int rc = App::Get().Run(hInstance, nCmdShow);
    DLog("main: run returned");
    Gdiplus::GdiplusShutdown(token);
    return rc;
}
