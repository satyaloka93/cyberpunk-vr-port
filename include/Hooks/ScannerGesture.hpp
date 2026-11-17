#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>

namespace cvr::input {
void PublishScannerVision(int state);
int ScannerVision(); // -1 means no recent game-state sample

// Scene cameras are not menus or braindances. They retain the ear gesture,
// while actual UI/BD owns the grips for its native shoulder actions.
inline bool ScannerGestureAllowed(bool enabled, bool menu, bool keypad, bool braindance,
                                  bool lobby, bool captured, bool wireOwnsGrip) {
    return enabled && !menu && !keypad && !braindance && !lobby && !captured && !wireOwnsGrip;
}

// World slow motion belongs to the world, not to tracked hands. Scanner and global time scale are independent
// reasons: leaving one must not resume physics while the other still holds it. Each transition invalidates old work.
class ScannerHandPhysics {
    std::atomic<uint64_t> state{0}; // low bits: scanner, time scale; upper bits: transition generation
    void Reason(uint64_t mask, bool active) {
        auto before=state.load(std::memory_order_acquire);
        for (;;) {
            const auto flags=active ? ((before&3)|mask) : ((before&3)&~mask);
            if(flags==(before&3))return;
            const auto after=((before&~uint64_t(3))+4)|flags;
            if(state.compare_exchange_weak(before,after,std::memory_order_acq_rel))return;
        }
    }
public:
    void Publish(int vision) { if(vision>=0)Reason(1,vision==1); }
    void TimeScale(float scale) { if(std::isfinite(scale) && scale>=0)Reason(2,scale<.999f); }
    uint64_t Epoch() const {
        const auto value=state.load(std::memory_order_acquire);
        return ((value>>2)<<1)|((value&3)!=0 ? 1 : 0);
    }
    bool ScannerActive() const { return (state.load(std::memory_order_acquire)&1)!=0; }
    bool Active() const { return (Epoch() & 1) != 0; }
    bool Accepts(uint64_t completed) const { return !(completed & 1) && Epoch() == completed; }
};
inline ScannerHandPhysics scannerHandPhysics;

class ScannerRequest {
    uint64_t started{};
    bool pending{},accepted{};
public:
    void Update(bool& requested,int vision,uint64_t now) {
        if(!requested){pending=accepted=false;started=0;return;}
        if(!pending){pending=true;started=now;accepted=false;}
        if(vision==1)accepted=true;
        // An input request is not proof that the game opened the scanner.
        // Release rejected/stale requests and respect a game-driven exit.
        if((accepted && vision==0) || (vision!=1 && now>=started && now-started>=750)) {
            requested=false;pending=accepted=false;started=0;
        }
    }
};
// Only a squeeze that STARTS at the ear counts. A hand already squeezing -- a shell or a magazine on
// its way from the belt to a gun held close in -- passes within the zone and must not open the scanner.
class ScannerSqueeze {
    bool was{},carried{};
public:
    bool Update(bool gripHeld,bool atEar) {
        if(gripHeld && !was)carried=!atEar;
        if(!gripHeld)carried=false;
        was=gripHeld;
        return gripHeld && !carried;
    }
    void Reset(){was=carried=false;}
};
// The runtime reports the controller's grip centre, below/outside the ear,
// rather than the point where the fingers touch the headset.
struct ScannerEarZone {
    float x=-0.18f, y=-0.08f, z=0.08f;
    float radius=0.22f, margin=0.04f;
    bool Contains(float px,float py,float pz,bool armed) const {
        if(!std::isfinite(px) || !std::isfinite(py) || !std::isfinite(pz) || px>-.04f)return false;
        const float r0=radius>.02f && radius<=.60f ? radius : .22f;
        const float extra=margin>=0 && margin<=.30f ? margin : .04f;
        const float r=r0+(armed?extra:0);
        const float dx=px-x,dy=py-y,dz=pz-z;
        return dx*dx+dy*dy+dz*dz<=r*r;
    }
};
}
