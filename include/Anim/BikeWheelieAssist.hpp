#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace cvr::anim {
inline float WheelieLimitDegrees(float v){return std::isfinite(v)?std::clamp(v,10.f,60.f):35.f;}
struct BikeAttitude {
    uint64_t stampUs{};uintptr_t vehicle{};
    float pitch{},rate{},upZ=1,roll{};
    int contacts{};
    bool valid{},frontContact{},rearContact{},wheelsValid{};
    bool scriptedTilt{}; // native quest/custom bank owns the vehicle
    float frontGap{},rearGap{};uint32_t wheelCount{};
};
// Exact ordered predicate used by the game's wheel +B4 query (RVA53BFF0).
// No-contact values (including unordered values) are not a failed memory read.
inline bool BikeNativeWheelContact(float gap) {return gap<=0.f;}
inline bool BikeContactFresh(const BikeAttitude& a,uint64_t now,uintptr_t owner) {
    return a.wheelsValid && owner && a.vehicle==owner && a.stampUs && now>=a.stampUs && now-a.stampUs<=100000;
}
inline bool BikeLeanGrounded(const BikeAttitude& a,uint64_t now,uintptr_t owner) {
    return BikeContactFresh(a,now,owner) && a.frontContact && a.rearContact && !a.scriptedTilt;
}
inline bool BikeMotionFresh(const BikeAttitude& a,uint64_t now,uintptr_t owner) {
    return a.valid && BikeContactFresh(a,now,owner);
}
inline bool BikeFrontLifted(const BikeAttitude& a,uint64_t now,uintptr_t owner) {
    return BikeContactFresh(a,now,owner) && !a.frontContact;
}
inline float BikeAirSteering(float barsAndLean,float lateralLean,bool directBank,bool frontLifted) {
    return frontLifted ? (directBank ? 0.f:lateralLean):barsAndLean;
}
inline bool BikePitchFromQuaternion(const float* q,float& pitch,float& upZ) {
    const float n=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
    if(!std::isfinite(n)||n<.5f||n>1.5f)return false;
    const float fy=1-2*(q[0]*q[0]+q[2]*q[2])/n,fx=2*(q[0]*q[1]-q[2]*q[3])/n;
    const float fz=2*(q[1]*q[2]+q[0]*q[3])/n;
    pitch=std::atan2(fz,std::hypot(fx,fy))*57.295779513f;
    upZ=1-2*(q[0]*q[0]+q[1]*q[1])/n;
    return std::isfinite(pitch)&&std::isfinite(upZ);
}
struct BikeAssistOutput {float lean{},throttle{},relativePitch{},predictedPitch{};bool active{};};
// Soft native-input assist: no transform, torque, brake or fabricated haptics.
// Does not promise a hard physical cap against collisions or existing momentum.
class BikeWheelieAssist {
public:
    void Reset(){*this={};}
    BikeAssistOutput Apply(float lean,float throttle,const BikeAttitude& a,uint64_t now,float limit) {
        BikeAssistOutput out{lean,throttle};
        if(!BikeMotionFresh(a,now,a.vehicle)||
           !std::isfinite(a.pitch)||!std::isfinite(a.rate)||!std::isfinite(a.upZ)||
           !std::isfinite(lean)||!std::isfinite(throttle)) {Reset();return out;}
        if(a.vehicle!=m_vehicle){Reset();m_vehicle=a.vehicle;}
        // Learn road pitch with both wheels on the ground. Hold that reference
        // once a wheel lifts, so a slope is not mistaken for the wheelie angle.
        if(a.frontContact && a.rearContact && a.upZ>.3f) {
            m_base=a.pitch;m_baseValid=true;
            m_armed=lean<-.15f; // remember the rider's initiation, even before throttle
            return out; // never taper the launch while both wheels are grounded
        }
        if(!a.rearContact) {m_armed=false;return out;} // native airborne control
        if(!m_baseValid)return out; // wait for a grounded baseline, never guess
        if(lean<-.15f && throttle>.05f)m_armed=true;
        if(!m_armed)return out;
        out.relativePitch=a.pitch-m_base;
        if(out.relativePitch<3 && lean>=-.05f){m_armed=false;return out;}
        limit=WheelieLimitDegrees(limit);
        // Anticipate rising pitch by 120ms, bounded to 18 degrees. Falling pitch
        // does not increase the request; a fast rise begins assistance earlier.
        out.predictedPitch=out.relativePitch+std::clamp(a.rate*.12f,0.f,18.f);
        const float risk=out.predictedPitch;
        const auto ramp=[](float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);};
        const float taper=ramp((risk-(limit-12.f))/12.f);
        if(out.lean<0)out.lean*=1-taper;
        const float counter=ramp((risk-limit)/10.f);
        if(counter>0)out.lean=std::max(out.lean,counter); // forward balance input
        out.throttle*=1-ramp((risk-(limit-6.f))/6.f);
        if(a.upZ<0) {out.throttle=0;out.lean=std::max(out.lean,.75f);}
        out.active=out.lean!=lean || out.throttle!=throttle;
        return out;
    }
private:
    uintptr_t m_vehicle{};
    float m_base{};bool m_baseValid{},m_armed{};
};
}
