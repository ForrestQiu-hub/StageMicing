#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "Pdf.h"

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

void AppendF(std::string& s, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) s.append(buf, static_cast<size_t>(n));
}

// POINTFX coordinates are 16.16 FIXED values in device pixels (a font
// selected with lfHeight = -100 reports a 100px em, so a capital is ~73).
double FixedToDouble(const FIXED& f) {
    return static_cast<double>(f.value) + static_cast<double>(f.fract) / 65536.0;
}

// StrokeRect/Line in points, top-left origin with the current content offset.
double PageY(double h, double offY, double y) { return h - (y + offY); }

}  // namespace

void BitmapToRgb(Gdiplus::Bitmap& bmp, std::vector<BYTE>& out) {
    const int w = static_cast<int>(bmp.GetWidth());
    const int h = static_cast<int>(bmp.GetHeight());
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData data{};
    if (bmp.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) !=
        Gdiplus::Ok)
        return;
    out.resize(static_cast<size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        const BYTE* row = static_cast<const BYTE*>(data.Scan0) + y * data.Stride;
        for (int x = 0; x < w; ++x) {
            const BYTE* p = row + x * 4;
            size_t o = (static_cast<size_t>(y) * w + x) * 3;
            out[o + 0] = p[2];
            out[o + 1] = p[1];
            out[o + 2] = p[0];
        }
    }
    bmp.UnlockBits(&data);
}

PdfDoc::PdfDoc(double widthPt, double heightPt) : w_(widthPt), h_(heightPt) {}

int PdfDoc::AddImage(int w, int h, const std::vector<BYTE>& rgb) {
    images_.push_back({w, h, false, rgb});
    return static_cast<int>(images_.size()) - 1;
}

int PdfDoc::AddJpeg(int w, int h, const std::vector<BYTE>& jpg) {
    images_.push_back({w, h, true, jpg});
    return static_cast<int>(images_.size()) - 1;
}

bool EncodeJpeg(Gdiplus::Bitmap& bmp, std::vector<BYTE>& out) {
    CLSID clsid{};
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(num, size, codecs);
    bool found = false;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(codecs[i].MimeType, L"image/jpeg") == 0) {
            clsid = codecs[i].Clsid;
            found = true;
            break;
        }
    }
    if (!found) return false;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, 0);
    if (!mem) return false;
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(mem, TRUE, &stream) != S_OK) {
        GlobalFree(mem);
        return false;
    }
    Gdiplus::EncoderParameters params{};
    ULONG q = 90;
    params.Count = 1;
    params.Parameter[0].Guid = Gdiplus::EncoderQuality;
    params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    params.Parameter[0].NumberOfValues = 1;
    params.Parameter[0].Value = &q;
    if (bmp.Save(stream, &clsid, &params) != Gdiplus::Ok) {
        stream->Release();
        return false;
    }
    STATSTG st{};
    stream->Stat(&st, STATFLAG_NONAME);
    ULONGLONG len = st.cbSize.QuadPart;
    out.resize(static_cast<size_t>(len));
    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    ULONG read = 0;
    stream->Read(out.data(), static_cast<ULONG>(len), &read);
    stream->Release();
    out.resize(read);
    return read > 0;
}

void PdfDoc::FillRect(double x, double y, double w, double h, PdfColor c) {
    AppendF(content_, "q %.4f %.4f %.4f rg %.3f %.3f %.3f %.3f re f Q\n", c.r / 255.0,
            c.g / 255.0, c.b / 255.0, x, h_ - (y + offY_) - h, w, h);
}

void PdfDoc::StrokeRect(double x, double y, double w, double h, double lw, PdfColor c) {
    AppendF(content_, "q %.4f %.4f %.4f RG %.2f w %.3f %.3f %.3f %.3f re S Q\n",
            c.r / 255.0, c.g / 255.0, c.b / 255.0, lw, x, PageY(h_, offY_, y) - h, w, h);
}

void PdfDoc::Line(double x1, double y1, double x2, double y2, double lw, PdfColor c) {
    AppendF(content_, "q %.4f %.4f %.4f RG %.2f w %.3f %.3f m %.3f %.3f l S Q\n",
            c.r / 255.0, c.g / 255.0, c.b / 255.0, lw, x1, PageY(h_, offY_, y1), x2,
            PageY(h_, offY_, y2));
}

void PdfDoc::Polyline(const std::vector<std::pair<double, double>>& pts, bool closed,
                      double lw, PdfColor c) {
    if (pts.size() < 2) return;
    AppendF(content_, "q %.4f %.4f %.4f RG %.2f w 1 J 1 j\n", c.r / 255.0, c.g / 255.0,
            c.b / 255.0, lw);
    AppendF(content_, "%.3f %.3f m\n", pts[0].first, h_ - (pts[0].second + offY_));
    for (size_t i = 1; i < pts.size(); ++i)
        AppendF(content_, "%.3f %.3f l\n", pts[i].first, h_ - (pts[i].second + offY_));
    if (closed) content_ += "h\n";
    content_ += "S Q\n";
}

void PdfDoc::PlaceImage(int id, double x, double y, double w, double h, double angleDeg) {
    const double rad = angleDeg * 3.14159265358979 / 180.0;
    const double ca = std::cos(rad), sa = std::sin(rad);
    const double a = w * ca, b = w * sa, c = -h * sa, d = h * ca;
    const double cx = x + w * 0.5;
    const double cy = h_ - (y + offY_ + h * 0.5);
    const double e = cx - (a + c) * 0.5;
    const double f = cy - (b + d) * 0.5;
    AppendF(content_, "q %.4f %.4f %.4f %.4f %.3f %.3f cm /Im%d Do Q\n", a, b, c, d, e, f, id);
}

void PdfDoc::TextImage(int id, double x, double y, double w, double h) {
    AppendF(content_, "q %.3f 0 0 %.3f %.3f %.3f cm /Im%d Do Q\n", w, h, x, h_ - (y + offY_) - h, id);
}

void PdfDoc::NewPage() {
    pages_.push_back(content_);   // retire the finished page's stream
    content_.clear();
    offY_ = 0;
}

bool PdfDoc::Save(const std::wstring& path) {
    // every finished page plus the one still being built
    std::vector<std::string> all = pages_;
    all.push_back(content_);
    const int nPages = (int)all.size();
    const int pageBase = 3;                       // page dict objects
    const int contBase = pageBase + nPages;       // content streams
    const int imgBase = contBase + nPages;        // images (shared by all pages)

    std::string pdf = "%PDF-1.4\n";
    std::vector<size_t> offs;
    auto obj = [&](const std::string& body) {
        offs.push_back(pdf.size());
        pdf += body;
    };
    // catalog, page tree, one dict per page, one content stream per page, images
    obj("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    {
        std::string body;
        AppendF(body, "2 0 obj\n<< /Type /Pages /Kids [");
        for (int i = 0; i < nPages; ++i)
            AppendF(body, "%d 0 R%s", pageBase + i, (i + 1 < nPages) ? " " : "");
        AppendF(body, "] /Count %d >>\nendobj\n", nPages);
        obj(body);
    }
    for (int i = 0; i < nPages; ++i) {
        std::string page;
        AppendF(page,
                "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] "
                "/Resources << /XObject << ",
                pageBase + i, w_, h_);
        for (size_t k = 0; k < images_.size(); ++k)
            AppendF(page, "/Im%zu %d 0 R ", k, imgBase + (int)k);
        AppendF(page,
                ">> /ProcSet [/PDF /ImageC] >> /Contents %d 0 R >>\nendobj\n",
                contBase + i);
        obj(page);
    }
    for (int i = 0; i < nPages; ++i) {
        offs.push_back(pdf.size());
        AppendF(pdf, "%d 0 obj\n<< /Length %zu >>\nstream\n", contBase + i,
                all[(size_t)i].size());
        pdf += all[(size_t)i];
        pdf += "\nendstream\nendobj\n";
    }
    for (size_t i = 0; i < images_.size(); ++i) {
        const Img& im = images_[i];
        offs.push_back(pdf.size());
        AppendF(pdf,
                "%d 0 obj\n<< /Type /XObject /Subtype /Image /Width %d /Height %d "
                "/ColorSpace /DeviceRGB /BitsPerComponent 8 %s/Length %zu >>\nstream\n",
                imgBase + (int)i, im.w, im.h, im.jpeg ? "/Filter /DCTDecode " : "",
                im.rgb.size());
        pdf.append(reinterpret_cast<const char*>(im.rgb.data()), im.rgb.size());
        pdf += "\nendstream\nendobj\n";
    }
    const size_t xref = pdf.size();
    char tail[128];
    snprintf(tail, sizeof(tail), "xref\n0 %zu\n0000000000 65535 f \n", offs.size() + 1);
    pdf += tail;
    for (size_t o : offs) {
        char line[40];
        snprintf(line, sizeof(line), "%010zu 00000 n \n", o);
        pdf += line;
    }
    snprintf(tail, sizeof(tail),
             "trailer\n<< /Size %zu /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n", offs.size() + 1,
             xref);
    pdf += tail;

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(pdf.data(), static_cast<std::streamsize>(pdf.size()));
    return out.good();
}
// True vector text: every glyph contour is emitted as PDF path operators and
// filled with the non-zero winding rule.  GGO_NATIVE reports points in device
// pixels (a 100px em selected via lfHeight = -100), so the outline is scaled by
// sizePt / 100 and its y axis is flipped into PDF page space.
void PdfDoc::GlyphText(const std::wstring& s, double x, double baselineY, double sizePt,
                       PdfColor c, bool bold) {
    if (s.empty() || sizePt <= 0.0) return;
    HDC dc = GetDC(nullptr);
    if (!dc) return;
    LOGFONTW lf{};
    lf.lfHeight = -100;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    HFONT font = CreateFontIndirectW(&lf);
    if (!font) {
        ReleaseDC(nullptr, dc);
        return;
    }
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);

    std::string path;
    const double scl = sizePt / 100.0;
    double cx = x;
    auto gx = [&](double v) { return cx + v * scl; };
    auto gy = [&](double v) { return PageY(h_, offY_, baselineY - v * scl); };
    auto charAdvance = [&](wchar_t ch) -> double {
        int wpx = 0;
        if (GetCharWidth32W(dc, ch, ch, &wpx) && wpx > 0) return static_cast<double>(wpx);
        return 0.0;
    };

    for (wchar_t ch : s) {
        MAT2 mat{};   // identity: FIXED is {fract = 0, value = 1}
        mat.eM11.fract = 0; mat.eM11.value = 1;
        mat.eM12.fract = 0; mat.eM12.value = 0;
        mat.eM21.fract = 0; mat.eM21.value = 0;
        mat.eM22.fract = 0; mat.eM22.value = 1;

        GLYPHMETRICS gm{};
        UINT fmt = GGO_NATIVE;
        DWORD sz = GetGlyphOutlineW(dc, ch, fmt, &gm, 0, nullptr, &mat);
        if (sz == GDI_ERROR) {
            fmt = GGO_BEZIER;
            sz = GetGlyphOutlineW(dc, ch, fmt, &gm, 0, nullptr, &mat);
        }
        double advance = 0.0;
        if (sz == GDI_ERROR) {
            advance = charAdvance(ch);
        } else {
            advance = static_cast<double>(gm.gmCellIncX);
            if (sz > 0) {
                std::vector<BYTE> buf(sz);
                if (GetGlyphOutlineW(dc, ch, fmt, &gm, sz, buf.data(), &mat) != GDI_ERROR) {
                    size_t off = 0;
                    while (off + sizeof(TTPOLYGONHEADER) <= buf.size()) {
                        const auto* ph =
                            reinterpret_cast<const TTPOLYGONHEADER*>(buf.data() + off);
                        if (ph->dwType != TT_POLYGON_TYPE) break;
                        if (ph->cb < sizeof(TTPOLYGONHEADER) || off + ph->cb > buf.size())
                            break;
                        const BYTE* p = buf.data() + off + sizeof(TTPOLYGONHEADER);
                        const BYTE* endp = buf.data() + off + ph->cb;
                        double curx = FixedToDouble(ph->pfxStart.x);
                        double cury = FixedToDouble(ph->pfxStart.y);
                        AppendF(path, "%.3f %.3f m\n", gx(curx), gy(cury));
                        while (p + offsetof(TTPOLYCURVE, apfx) <= endp) {
                            const auto* cv = reinterpret_cast<const TTPOLYCURVE*>(p);
                            const size_t need = offsetof(TTPOLYCURVE, apfx) +
                                                static_cast<size_t>(cv->cpfx) *
                                                    sizeof(POINTFX);
                            if (p + need > endp) break;
                            const POINTFX* ap = cv->apfx;
                            if (cv->wType == TT_PRIM_QSPLINE && cv->cpfx >= 2) {
                                for (WORD i = 0; i + 1 < cv->cpfx; ++i) {
                                    const double qx = FixedToDouble(ap[i].x);
                                    const double qy = FixedToDouble(ap[i].y);
                                    double ex, ey;
                                    if (i + 2 == cv->cpfx) {
                                        ex = FixedToDouble(ap[i + 1].x);
                                        ey = FixedToDouble(ap[i + 1].y);
                                    } else {
                                        ex = 0.5 * (qx + FixedToDouble(ap[i + 1].x));
                                        ey = 0.5 * (qy + FixedToDouble(ap[i + 1].y));
                                    }
                                    // quadratic control point -> cubic control points
                                    const double c1x = curx + (2.0 / 3.0) * (qx - curx);
                                    const double c1y = cury + (2.0 / 3.0) * (qy - cury);
                                    const double c2x = ex + (2.0 / 3.0) * (qx - ex);
                                    const double c2y = ey + (2.0 / 3.0) * (qy - ey);
                                    AppendF(path,
                                            "%.3f %.3f %.3f %.3f %.3f %.3f c\n", gx(c1x),
                                            gy(c1y), gx(c2x), gy(c2y), gx(ex), gy(ey));
                                    curx = ex;
                                    cury = ey;
                                }
                            } else if (cv->wType == TT_PRIM_CSPLINE && cv->cpfx >= 3) {
                                for (WORD i = 0; i + 2 < cv->cpfx; i += 3) {
                                    AppendF(path,
                                            "%.3f %.3f %.3f %.3f %.3f %.3f c\n",
                                            gx(FixedToDouble(ap[i].x)),
                                            gy(FixedToDouble(ap[i].y)),
                                            gx(FixedToDouble(ap[i + 1].x)),
                                            gy(FixedToDouble(ap[i + 1].y)),
                                            gx(FixedToDouble(ap[i + 2].x)),
                                            gy(FixedToDouble(ap[i + 2].y)));
                                    curx = FixedToDouble(ap[i + 2].x);
                                    cury = FixedToDouble(ap[i + 2].y);
                                }
                            } else {   // TT_PRIM_LINE, or a lone on-curve point
                                for (WORD i = 0; i < cv->cpfx; ++i) {
                                    const double px = FixedToDouble(ap[i].x);
                                    const double py = FixedToDouble(ap[i].y);
                                    AppendF(path, "%.3f %.3f l\n", gx(px), gy(py));
                                    curx = px;
                                    cury = py;
                                }
                            }
                            p += need;
                        }
                        path += "h\n";
                        off += ph->cb;
                    }
                }
            }
        }
        cx += advance * scl;
    }

    SelectObject(dc, old);
    DeleteObject(font);
    ReleaseDC(nullptr, dc);

    if (path.empty()) return;
    AppendF(content_, "q %.4f %.4f %.4f rg\n", c.r / 255.0, c.g / 255.0, c.b / 255.0);
    content_ += path;
    content_ += "f\nQ\n";
}