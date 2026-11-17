#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cvr::bikebank {
inline constexpr float kRadPerDegree=.017453292519943295f;
inline float Gain(float value) {return std::isfinite(value)?std::clamp(value,0.f,2.f):1.f;}
inline float MaxAddedDegrees(float value) {return std::isfinite(value)?std::clamp(value,0.f,45.f):20.f;}
// Head tilt is angular 1:1 after its deadzone. Body translation estimates a
// lean angle about a point 50 cm below the neck; it is not measured torso roll.
inline float RequestedDegrees(bool headTilt,bool tiltValid,float tiltDegrees,float bodyMeters,
                              float tiltDead,float gain,float maximum) {
    float angle=0;
    if(headTilt) {
        if(!tiltValid || !std::isfinite(tiltDegrees))return 0;
        const float dead=std::isfinite(tiltDead)?std::clamp(tiltDead,0.f,8.f):3.f;
        angle=std::copysign(std::max(std::abs(tiltDegrees)-dead,0.f),tiltDegrees);
    } else {
        if(!std::isfinite(bodyMeters))return 0;
        const float sideways=std::copysign(std::max(std::abs(bodyMeters)-.03f,0.f),bodyMeters);
        angle=std::atan2(sideways,.50f)/kRadPerDegree;
    }
    const float limit=MaxAddedDegrees(maximum);
    return std::clamp(angle*Gain(gain),-limit,limit);
}
inline bool Fresh(uint64_t now,uint64_t stamp) {return stamp && now>=stamp && now-stamp<=100000;}
// Preserve the engine's target exactly when inactive/invalid. The result goes
// into its existing PID/tilt-speed controller, never directly into a transform.
inline float AddToNative(float nativeRadians,float addedDegrees,float nativeMaxDegrees) {
    if(!std::isfinite(nativeRadians) || !std::isfinite(addedDegrees) || addedDegrees==0 ||
       !std::isfinite(nativeMaxDegrees) || nativeMaxDegrees<1 || nativeMaxDegrees>89)return nativeRadians;
    const float limit=nativeMaxDegrees*kRadPerDegree;
    if(std::abs(nativeRadians)>limit+.0001f)return nativeRadians;
    return std::clamp(nativeRadians+std::clamp(addedDegrees,-45.f,45.f)*kRadPerDegree,-limit,limit);
}
// A gentle turn through native speed-dependent steering; no sideways force.
// Default strength .5 gives 25% steering at 20 degrees of physical bank.
inline float LeanSteering(float degrees,float strength) {
    if(!std::isfinite(degrees))return 0;
    strength=std::isfinite(strength)?std::clamp(strength,0.f,1.f):.5f;
    return std::clamp(degrees*strength/40.f,-.35f,.35f);
}
// Steering already requests bank: use the stronger same-side request, not both
// added together. Opposing lean still counters the native steering bank.
inline float CoordinatedTarget(float nativeRadians,float leanDegrees,float maximum) {
    if(!std::isfinite(nativeRadians)||!std::isfinite(leanDegrees)||leanDegrees==0||
       !std::isfinite(maximum)||maximum<1||maximum>89)return nativeRadians;
    const float limit=maximum*kRadPerDegree;
    if(std::abs(nativeRadians)>limit+.0001f)return nativeRadians;
    const float lean=std::clamp(leanDegrees,-45.f,45.f)*kRadPerDegree;
    const float target=nativeRadians*lean>=0 ? std::copysign(std::max(std::abs(nativeRadians),std::abs(lean)),lean)
                                           : nativeRadians+lean;
    return std::clamp(target,-limit,limit);
}
// Both lean steering and bank share this monotonic ramp. Timestamp, not poll
// count, controls the rate. Recenter, vehicle changes and stale input reset it.
class LeanRamp {
public:
    void Reset(){*this={};}
    float Update(float wanted,uint64_t stamp,uint64_t origin,uintptr_t vehicle) {
        if(!std::isfinite(wanted)||!stamp||!origin||!vehicle){Reset();return 0;}
        if(origin!=m_origin||vehicle!=m_vehicle||stamp<m_stamp||stamp-m_stamp>100000){
            Reset();m_stamp=stamp;m_origin=origin;m_vehicle=vehicle;return 0;
        }
        const float step=90.f*float(stamp-m_stamp)*1e-6f;
        m_stamp=stamp;m_value+=std::clamp(std::clamp(wanted,-45.f,45.f)-m_value,-step,step);
        return m_value;
    }
private:
    float m_value{};uint64_t m_stamp{},m_origin{};uintptr_t m_vehicle{};
};

}
