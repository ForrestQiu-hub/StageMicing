#include "Assets.h"
#include <algorithm>

Assets& Assets::Get() {
    static Assets a;
    return a;
}

void Assets::Init(const std::wstring& exeDir) {
    root_ = exeDir;
}

Gdiplus::Bitmap* Assets::A(const std::wstring& rel) {
    auto it = cache_.find(rel);
    if (it != cache_.end()) return it->second;

    std::wstring path = root_ + L"\\assets\\" + rel;
    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromFile(path.c_str(), FALSE);
    if (bmp && bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        bmp = nullptr;
    }
    cache_[rel] = bmp;
    return bmp;
}

Gdiplus::Bitmap* Assets::ItemIcon(const std::wstring& id, bool mic) {
    return A((mic ? L"mics\\" : L"instruments\\") + id + L".png");
}

Gdiplus::Bitmap* Assets::AppIcon() {
    return A(L"app.png");
}

Gdiplus::Bitmap* Assets::TintIcon(const std::wstring& id, bool mic,
                                  const Rgb& color) {
    std::wstring key = (mic ? L"m\\" : L"i\\") + id + L"|" +
                       std::to_wstring(color.r) + L"," +
                       std::to_wstring(color.g) + L"," +
                       std::to_wstring(color.b);
    auto it = tintCache_.find(key);
    if (it != tintCache_.end()) return it->second;

    Gdiplus::Bitmap* src = ItemIcon(id, mic);
    if (!src) {
        tintCache_[key] = nullptr;
        return nullptr;
    }

    // shrink to a sane maximum so tinting stays cheap
    const int kMax = 256;
    int sw = (int)src->GetWidth();
    int sh = (int)src->GetHeight();
    float sc = std::min(1.f, std::min((float)kMax / sw, (float)kMax / sh));
    int w = std::max(1, (int)(sw * sc + 0.5f));
    int h = std::max(1, (int)(sh * sc + 0.5f));

    Gdiplus::Bitmap* dst = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(dst);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.DrawImage(src, 0, 0, w, h);
    }

    Gdiplus::BitmapData bd;
    Gdiplus::Rect rc(0, 0, w, h);
    if (dst->LockBits(&rc, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite,
                      PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        delete dst;
        tintCache_[key] = nullptr;
        return nullptr;
    }
    for (int y = 0; y < h; ++y) {
        BYTE* row = (BYTE*)bd.Scan0 + (size_t)y * bd.Stride;
        for (int x = 0; x < w; ++x) {
            BYTE* p = row + x * 4;
            p[0] = color.b;
            p[1] = color.g;
            p[2] = color.r;
            // p[3] (alpha) stays untouched
        }
    }
    dst->UnlockBits(&bd);

    tintCache_[key] = dst;
    return dst;
}
