#pragma once
#include "Anim/BikeWheelieAssist.hpp"

namespace cvr::haptics {
struct BikeContactEffect {
    bool muteRoad{};
    float amplitude{};
    int durationMs{};
    unsigned event{}; // 1 liftoff, 2 touchdown; never a replayed initial state
};
// Consumed on the XR haptic thread. Confirmation uses advancing PHYSICS sample
// time, not repeated render polls of the same sample. Independent cooldowns let
// a short wheelie still produce both liftoff and landing feedback.
class BikeContactHaptics {
public:
    void Reset(){*this={};}
    BikeContactEffect Update(const anim::BikeAttitude& a,uint64_t now,uintptr_t owner,bool enabled) {
        BikeContactEffect out{};
        if(!enabled || !anim::BikeContactFresh(a,now,owner)) {Reset();return out;}
        const unsigned raw=(a.frontContact?1u:0u)|(a.rearContact?2u:0u);
        out.muteRoad=!(raw&1u); // immediate front-lift suppression, also when fully airborne
        if(!m_valid || m_owner!=owner || a.stampUs<m_stamp) {
            Reset();m_valid=true;m_owner=owner;m_stable=m_candidate=raw;m_since=a.stampUs;m_stamp=a.stampUs;
            return out;
        }
        m_stamp=a.stampUs;
        if(raw!=m_candidate){m_candidate=raw;m_since=a.stampUs;}
        // Do not restore road rumble during a transient front-contact flicker.
        out.muteRoad=out.muteRoad || !(m_stable&1u);
        if(raw==m_stable || a.stampUs-m_since<30000)return out;
        const unsigned landed=raw&~m_stable,lifted=m_stable&~raw;
        m_stable=raw;out.muteRoad=!(raw&1u);
        if(landed && (!m_lastLand || a.stampUs-m_lastLand>=150000)) {
            m_lastLand=a.stampUs;out.event=2;
            out.amplitude=landed==3 ? .9f:.8f;out.durationMs=125;
        } else if(lifted && (!m_lastLift || a.stampUs-m_lastLift>=150000)) {
            m_lastLift=a.stampUs;out.event=1;out.amplitude=.55f;out.durationMs=75;
        }
        return out;
    }
private:
    uintptr_t m_owner{};
    uint64_t m_stamp{},m_since{},m_lastLand{},m_lastLift{};
    unsigned m_stable{},m_candidate{};bool m_valid{};
};
}
