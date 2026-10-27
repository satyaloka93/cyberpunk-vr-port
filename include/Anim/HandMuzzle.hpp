#pragma once
#include <cstdint>
#include <cmath>
#include <mutex>
namespace cvr::aim {
struct HandMuzzle { uintptr_t weapon{}; float p[3]{}, q[4]{}, f[3]{}; uint64_t stamp{}; int hand{-1}; };
inline std::mutex handMuzzleMutex;
inline HandMuzzle handMuzzles[2];
inline bool Fresh(const HandMuzzle& m,uint64_t now) { return m.weapon && now>=m.stamp && now-m.stamp<=150; }
inline bool PublishHand(HandMuzzle m) {
    if(m.hand<0 || m.hand>1 || !m.weapon)return false;
    float n=0;for(float v:m.q){if(!std::isfinite(v))return false;n+=v*v;}
    for(float v:m.p)if(!std::isfinite(v))return false;
    if(n<.5f || n>1.5f)return false;
    n=1/std::sqrt(n);for(float& v:m.q)v*=n;
    auto [i,j,k,r]=m.q;
    m.f[0]=2*(i*j-k*r);m.f[1]=1-2*(i*i+k*k);m.f[2]=2*(j*k+i*r);
    std::lock_guard<std::mutex> lock(handMuzzleMutex);handMuzzles[m.hand]=m;return true;
}
inline bool ForWeapon(uintptr_t weapon,uint64_t now,HandMuzzle& out) {
    if(!weapon)return false;
    std::lock_guard<std::mutex> lock(handMuzzleMutex);
    for(const auto& m:handMuzzles)if(m.weapon==weapon && Fresh(m,now)){out=m;return true;}
    return false;
}
inline bool DualMuzzles(uint64_t now) {
    std::lock_guard<std::mutex> lock(handMuzzleMutex);
    return Fresh(handMuzzles[0],now)&&Fresh(handMuzzles[1],now)&&handMuzzles[0].weapon!=handMuzzles[1].weapon;
}
}
