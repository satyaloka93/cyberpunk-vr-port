#pragma once
#include "Anim/TrackedWheelSteering.hpp"
#include "Anim/BikeWheelieAssist.hpp"

namespace cvr::anim {
inline float BikeStuntFullMeters(float value) {
    return std::isfinite(value)?std::clamp(value,.08f,.35f):.15f;
}
inline float BikeStuntDeadMeters(float value,float full) {
    return std::isfinite(value)?std::clamp(value,0.f,std::min(.08f,BikeStuntFullMeters(full)-.01f)):.03f;
}
inline float BikeStuntAxis(float meters,float full,float dead) {
    if(!std::isfinite(meters))return 0;
    full=BikeStuntFullMeters(full);dead=BikeStuntDeadMeters(dead,full);
    const float amount=std::clamp((std::abs(meters)-dead)/(full-dead),0.f,1.f);
    return amount>0 ? std::copysign(amount,meters):0;
}
// Explicit stick input wins, including input opposite to the physical lean.
// No additive saturation: the rider can counter a physical lean with the stick.
inline float BikeStuntMerge(float padY,float encodedLean) {
    if(!std::isfinite(encodedLean)||encodedLean==0 || std::abs(padY)>.15f)return padY;
    return std::clamp(encodedLean,-1.f,1.f);
}
// Longitudinal weight shift in the grab-time horizontal facing, not head pitch.
// The estimated neck removes ordinary looking/nodding motion around the neck.
class TrackedBikeStunts {
public:
    static constexpr uint64_t kFreshUs=100000;
    void Reset(){*this={};}
    void Suspend(){m_valid=false;}
    bool Fresh(uint64_t now)const{return m_valid&&now>=m_stamp&&now-m_stamp<=kFreshUs;}
    uint64_t Stamp()const{return m_stamp;}
    uintptr_t Vehicle()const{return m_vehicle;}
    uint64_t Origin()const{return m_origin;}
    float Meters()const{return m_meters;}
    bool HandsValid(uint64_t now)const{return Fresh(now)&&m_handsValid;}
    float HandHeight()const{return m_handHeight;}
    float Axis(uint64_t now,float full,float dead)const {
        return Fresh(now)?BikeStuntAxis(m_meters,full,dead):0;
    }
    void Update(uint8_t held,const WheelTrackingFrame& frame,uint64_t now,uintptr_t vehicle) {
        m_valid=false;m_handsValid=false;
        if(!(held&3)||!vehicle){Reset();return;}
        if(m_vehicle!=vehicle || (m_origin && frame.origin && m_origin!=frame.origin))Reset();
        m_vehicle=vehicle;
        const auto& q=frame.head.orientation;
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        if(!(held&frame.validHands&3)||!frame.headValid||!frame.origin||!frame.sequence||!frame.stampUs||
           now<frame.stampUs||now-frame.stampUs>kFreshUs||!Finite(frame.head.position)||
           !std::isfinite(norm)||norm<.5f||norm>1.5f)return;
        if(m_stamp && (frame.stampUs<m_stamp || (frame.sequence!=m_sequence && frame.stampUs<=m_stamp)))return;
        const float scale=1/std::sqrt(norm);
        const XrQuaternionf rotation{q.x*scale,q.y*scale,q.z*scale,q.w*scale};
        const auto offset=RotateVector(rotation,XrVector3f{0,-.10f,.10f});
        const XrVector3f neck{frame.head.position.x+offset.x,frame.head.position.y+offset.y,frame.head.position.z+offset.z};
        if(!m_neutralValid) {
            const auto forward=RotateVector(rotation,XrVector3f{0,0,-1});
            const float horizontal=std::hypot(forward.x,forward.z);
            if(horizontal<.258819045f)return;
            m_forward={forward.x/horizontal,0,forward.z/horizontal};
            m_neutral=neck;m_neutralValid=true;
        }
        m_meters=(neck.x-m_neutral.x)*m_forward.x+(neck.z-m_neutral.z)*m_forward.z;
        // A large tracking-space discontinuity is not a stunt. Regrab/recenter
        // establishes a new reference; returning to the old reference is safe.
        if(std::abs(m_meters)>.60f)return;
        if((held&frame.validHands&3)==3 && Finite(frame.hands[0]) && Finite(frame.hands[1])) {
            const auto r=RotateVector(rotation,frame.hands[0]);
            const auto l=RotateVector(rotation,frame.hands[1]);
            m_handHeight=(r.y+l.y)*.5f-offset.y;
            m_handsValid=std::isfinite(m_handHeight)&&std::abs(m_handHeight)<1.5f;
        }
        m_origin=frame.origin;m_stamp=frame.stampUs;m_sequence=frame.sequence;m_valid=true;
    }
private:
    static bool Finite(XrVector3f p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    XrVector3f m_forward{},m_neutral{};
    uint64_t m_origin{},m_stamp{},m_sequence{};
    uintptr_t m_vehicle{};
    float m_meters{},m_handHeight{};
    bool m_handsValid{};
    bool m_valid{},m_neutralValid{};
};
inline float BikeHandFullMeters(float v){return std::isfinite(v)?std::clamp(v,.06f,.25f):.12f;}
class BikeHandBalance {
public:
    void Reset(){*this={};}
    bool Active()const{return m_active;}
    float Travel()const{return m_travel;}
    float Apply(float body,float height,bool hands,const BikeAttitude& a,uint64_t now,float full,uint64_t origin=1) {
        if(!origin || !hands || !BikeContactFresh(a,now,a.vehicle) || !a.rearContact) {Reset();return body;}
        if(a.vehicle!=m_vehicle || origin!=m_origin){Reset();m_vehicle=a.vehicle;m_origin=origin;}
        if(a.frontContact) {
            // A single front-wheel contact during a wheelie must not erase the
            // hand reference. Confirm landing on advancing physics samples.
            if(!m_active || (m_landedAt && a.stampUs-m_landedAt>=30000)) {
                m_active=false;m_travel=0;m_ground=true;m_neutral=height;m_base=body;m_landedAt=0;
                return body;
            }
            if(!m_landedAt)m_landedAt=a.stampUs;
        } else m_landedAt=0;
        if(!m_ground)return body;
        if(!m_active){
            if(m_base>=-.15f)return body;
            m_active=true;m_liftAt=a.stampUs;
        }
        m_travel=height-m_neutral;
        if(!std::isfinite(m_travel)||std::abs(m_travel)>.5f){Reset();return body;}
        // Blend out launch lean over 180ms. Neutral hands then request neutral
        // weight shift, not perpetual full rearward input. Throttle still owns
        // acceleration; up/down hands span one axis each way (no doubled gain).
        const float hand=std::copysign(std::clamp((std::abs(m_travel)-.015f)/(BikeHandFullMeters(full)-.015f),0.f,1.f),m_travel);
        const float blend=std::clamp(float(a.stampUs-m_liftAt)/180000.f,0.f,1.f);
        const float result=std::clamp(m_base*(1-blend)-hand,-1.f,1.f);
        // A deliberate forward body shift always brings the front down.
        return body>.05f ? std::max(body,result):result;
    }
private:
    uintptr_t m_vehicle{};uint64_t m_origin{},m_liftAt{},m_landedAt{};float m_neutral{},m_base{},m_travel{};
    bool m_ground{},m_active{};
};
}
