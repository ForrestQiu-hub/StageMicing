#pragma once
#include <cstdio>
#include <windows.h>

// Temporary startup tracer.
inline void DLog(const char* msg) {
    FILE* f = nullptr;
    if (fopen_s(&f, "E:\\Stage Micing Scene\\build\\runlog.txt", "a") == 0 && f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

inline void DLogW(const wchar_t* msg) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, L"E:\\Stage Micing Scene\\build\\runlog.txt", L"a") == 0 && f) {
        fwprintf(f, L"%s\n", msg);
        fclose(f);
    }
}
