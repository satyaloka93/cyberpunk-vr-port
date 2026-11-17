#pragma once
#include <cmath>
#include <cstring>
#include <initializer_list>
namespace cvr::lobby {
// Preserve upstream's 72-byte match (last 8 bytes are unwritten padding).
// The earlier measured 2.5 variant only duplicates the first 32 bytes; the
// remaining 48 may be zero or stale words. Its finite, non-denormal leading
// grading rows are stable. Caller retains weights/dimensions/saturation guards.
inline bool CompositionGradingMatches(const float* b) {
    if(std::memcmp(b+16,b+88,72)==0)return true;
    if(std::memcmp(b+16,b+88,32)!=0)return false;
    bool nonzero=false;
    for(unsigned i=16;i<24;++i) {
        if(!std::isfinite(b[i]))return false;
        if(b[i]!=0) {if(!std::isnormal(b[i]))return false;nonzero=true;}
    }
    return nonzero;
}
inline int CompositionBlock(const float* b) {
    const auto weight = [](float v) { return v == 0.0f || (v >= 1e-3f && v <= 1.0f); };
    for (const int i : {1 * 4 + 0, 1 * 4 + 1, 1 * 4 + 2, 1 * 4 + 3, 2 * 4 + 0})
        if (!weight(b[i])) return 2;
    const float w = b[16 * 4 + 2], h = b[16 * 4 + 3];
    if (!(w >= 64.0f && w <= 16384.0f && h >= 64.0f && h <= 16384.0f) || w != float(int(w)) || h != float(int(h)))
        return 3;
    if (!CompositionGradingMatches(b)) return 4;
    // The grading is real: saturation set, aberration tiny.
    const float sat = b[6 * 4 + 2], ab = b[6 * 4 + 3];
    return sat >= 1e-3f && sat <= 4.0f && ab >= 0.0f && ab < 0.01f ? 0 : 5;
}

}
