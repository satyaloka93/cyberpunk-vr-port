#pragma once
#include <cmath>
#include <cstdint>
#include <mutex>
namespace cvr::aim {
struct BarrelHit { float p[3]{}; uint64_t stamp{}; };
inline std::mutex hitMutex;
inline BarrelHit hit;
inline void PublishHit(float x,float y,float z,uint64_t now) {
    std::lock_guard lock(hitMutex);
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) { hit={}; return; }
    hit={{x,y,z},now};
}
inline bool SurfaceDistance(const BarrelHit& h,const float* origin,const float* forward,uint64_t now,float& distance) {
    if (!h.stamp || now<h.stamp || now-h.stamp>100) return false;
    float d[3]{h.p[0]-origin[0],h.p[1]-origin[1],h.p[2]-origin[2]};
    const float n=forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2];
    if (!(n>.9f && n<1.1f)) return false;
    const float t=(d[0]*forward[0]+d[1]*forward[1]+d[2]*forward[2])/n;
    if (!std::isfinite(t)||t<.1f||t>200.f) return false;
    float error=0;
    for(int i=0;i<3;++i) { const float e=d[i]-forward[i]*t; error+=e*e; }
    const float tolerance=.03f+t*.005f;
    if (!std::isfinite(error)||error>tolerance*tolerance) return false;
    distance=t;return true;
}
inline bool ReadSurfaceDistance(const float* origin,const float* forward,uint64_t now,float& distance) {
    BarrelHit snapshot;
    { std::lock_guard lock(hitMutex); snapshot=hit; }
    return SurfaceDistance(snapshot,origin,forward,now,distance);
}
}
