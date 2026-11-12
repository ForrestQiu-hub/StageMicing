#pragma once

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <string>
#include <vector>

struct PdfColor {
    int r, g, b;
};

// Minimal single-page PDF builder: vector shapes plus embedded raster objects.
// Coordinates are in points with the origin at the TOP-LEFT of the page.
class PdfDoc {
public:
    PdfDoc(double widthPt, double heightPt);

    int AddImage(int w, int h, const std::vector<BYTE>& rgb);
    int AddJpeg(int w, int h, const std::vector<BYTE>& jpg);

    void FillRect(double x, double y, double w, double h, PdfColor c);
    void StrokeRect(double x, double y, double w, double h, double lw, PdfColor c);
    void Line(double x1, double y1, double x2, double y2, double lw, PdfColor c);
    void Polyline(const std::vector<std::pair<double,double>>& pts, bool closed, double lw,
                  PdfColor c);
    void PlaceImage(int id, double x, double y, double w, double h, double angleDeg);
    void TextImage(int id, double x, double y, double w, double h);

    // true vector text: glyph outlines filled as paths (no font embedding)
    void GlyphText(const std::wstring& s, double x, double baselineY, double sizePt,
                   PdfColor c, bool bold);

    bool Save(const std::wstring& path);

    // Multi-page output: NewPage() closes the current content stream and starts a
    // fresh page with the SAME media box; Save() writes every finished page (plus
    // the current one) into one document with a /Count N page tree.
    void NewPage();
    int PageCount() const { return (int)pages_.size() + 1; }

    void SetOffset(double dy) { offY_ = dy; }

    double Width() const { return w_; }
    double Height() const { return h_; }

private:
    double w_ = 0, h_ = 0;
    double offY_ = 0;
    std::string content_;
    struct Img {
        int w, h;
        bool jpeg = false;
        std::vector<BYTE> rgb;
    };
    std::vector<Img> images_;
    std::vector<std::string> pages_;   // finished page content streams
};

// Converts a GDI+ bitmap to tightly packed RGB bytes.
void BitmapToRgb(Gdiplus::Bitmap& bmp, std::vector<BYTE>& out);
bool EncodeJpeg(Gdiplus::Bitmap& bmp, std::vector<BYTE>& out);
