#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <map>
#include "Model.h"

class Assets {
public:
    static Assets& Get();

    void Init(const std::wstring& exeDir);
    // rel is relative to the assets folder, e.g. L"instruments/violin.png"
    Gdiplus::Bitmap* A(const std::wstring& rel);
    Gdiplus::Bitmap* ItemIcon(const std::wstring& id, bool mic);
    Gdiplus::Bitmap* AppIcon();

    // Pre-tinted copy (max 256 px, straight alpha, RGB = colour everywhere).
    // Drawn with plain DrawImage so the transparent background stays clear.
    Gdiplus::Bitmap* TintIcon(const std::wstring& id, bool mic, const Rgb& color);

private:
    std::wstring root_;
    std::map<std::wstring, Gdiplus::Bitmap*> cache_;
    std::map<std::wstring, Gdiplus::Bitmap*> tintCache_;
};
