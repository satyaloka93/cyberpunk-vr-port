#pragma once
#include "Anim/BikeStunts.hpp"

namespace cvr::anim {
inline float WheelieHoldDegrees(float value) {
    return std::isfinite(value)?std::clamp(value,5.f,45.f):25.f;
}
inline float WheelieHoldCeiling(float limit) {return std::clamp(WheelieLimitDegrees(limit)-8.f,5.f,45.f);}
struct BikeHoldOutput {float lean{},target{},relativePitch{},handTravel{};bool active{};};
// Controls native fore/aft weight shift only. The rider owns throttle/brake.
// Grounded launch is unmodified; a fresh, deliberate wheelie is required.
class BikeWheelieHold {
public:
    void Reset(){*this={};}
    BikeHoldOutput Apply(float body,float height,bool hands,float throttle,float brake,
                         const BikeAttitude& a,uint64_t now,float handFull,uint64_t origin,
                         float angle,float limit) {
        BikeHoldOutput out{std::isfinite(body)?body:0.f};
        if(!origin || !hands || !BikeMotionFresh(a,now,a.vehicle) ||
           !std::isfinite(body)||!std::isfinite(height)||!std::isfinite(throttle)||!std::isfinite(brake)||
           !std::isfinite(a.pitch)||!std::isfinite(a.rate)||!std::isfinite(a.upZ)||a.upZ<.3f) {Reset();return out;}
        if(a.vehicle!=m_vehicle || origin!=m_origin || (m_stamp && a.stampUs<m_stamp)) {
            Reset();m_vehicle=a.vehicle;m_origin=origin;
        }
        // Cancellation precedes cached-sample reuse: braking, throttle release
        // or forward body input must work on the very next input poll.
        if(brake>.08f || throttle<.05f || body>.10f || !a.rearContact) {
            Reset();return out;
        }
        if(a.frontContact) {
            if(!m_active || (m_landSince && a.stampUs-m_landSince>=30000)) {
                m_basePitch=a.pitch;m_neutral=height;m_ground=true;m_armed=body<-.15f;
                m_active=false;m_integral=0;m_liftSince=0;m_landSince=0;m_stamp=a.stampUs;
                return out;
            }
            if(!m_landSince)m_landSince=a.stampUs;
        } else m_landSince=0;
        if(!m_ground || !m_armed)return out;
        const float pitch=a.pitch-m_basePitch;
        const float travel=height-m_neutral;
        if(!std::isfinite(travel)||std::abs(travel)>.5f){Reset();return out;}
        if(!m_active) {
            if(!m_liftSince)m_liftSince=a.stampUs;
            if(a.stampUs-m_liftSince<30000 || pitch<8.f)return out;
            m_active=true;m_output=body;m_integral=0;m_stamp=a.stampUs;
            // Catch at the current pitch, then move toward the chosen angle.
            // This avoids a step demand at the instant contact is lost.
            m_target=std::clamp(pitch,5.f,WheelieHoldCeiling(limit));
        }
        const float dt=std::clamp(float(a.stampUs-m_stamp)*1e-6f,0.f,.05f);
        const float trim=std::copysign(std::clamp((std::abs(travel)-.015f)/(BikeHandFullMeters(handFull)-.015f),0.f,1.f),travel);
        const float desired=std::clamp(WheelieHoldDegrees(angle)+12.f*trim,5.f,WheelieHoldCeiling(limit));
        // Bounded target slew and output slew keep input continuous across
        // different physics rates. Repeated input polls cannot advance time.
        m_target+=std::clamp(desired-m_target,-30.f*dt,30.f*dt);
        const float error=pitch-m_target;
        const float pd=.045f*error+.007f*std::clamp(a.rate,-180.f,180.f);
        const float proposed=std::clamp(m_integral+.020f*error*dt,-.35f,.35f);
        // Integrate only when it will not wind further into a saturated input.
        if(std::abs(pd+proposed)<.85f || (pd+proposed)*error<0)m_integral=proposed;
        const float control=std::clamp(pd+m_integral,-.85f,.85f);
        m_output+=std::clamp(control-m_output,-6.f*dt,6.f*dt);
        m_stamp=a.stampUs;
        return {m_output,m_target,pitch,travel,true};
    }
private:
    uintptr_t m_vehicle{};uint64_t m_origin{},m_stamp{},m_liftSince{},m_landSince{};
    float m_basePitch{},m_neutral{},m_target{},m_integral{},m_output{};
    bool m_ground{},m_armed{},m_active{};
};
}
