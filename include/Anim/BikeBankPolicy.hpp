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
}
