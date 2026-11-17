#pragma once
#include <windows.h>
#include <cstdint>
inline uint64_t XrDiagNowUs(){LARGE_INTEGER t{},f{};QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return uint64_t(t.QuadPart*1000000ll/f.QuadPart);}
class OpenXRManager {
public:
    static OpenXRManager& Get() { static OpenXRManager manager; return manager; }
    bool IsSessionRunning() const { return true; }
    int ApplyHaptic(int hand, float amplitude, float durationSec);
};
