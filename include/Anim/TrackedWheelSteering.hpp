#pragma once
#include "Anim/WheelSteering.hpp"
#include "Anim/WheelPrediction.hpp"
#include "Utils/XrMath.hpp"

namespace cvr::anim {
struct WheelTrackingFrame {
    XrPosef head{{0,0,0,1},{}};
    XrVector3f hands[2]{}; // head-relative, right then left
    uint64_t sequence{},origin{},stampUs{};
    uint8_t validHands{};
    bool headValid{};
};
// THE WHEEL THE HANDS HOLD, through gaps. `held` is the hands that OWN the wheel (their grip closed on it); a grab ends
// only when that ownership ends (held 0), the caller resets on a new grab, and a recenter starts a new tracking space.
// Tracking that goes stale, a hand that drops out of tracking, a publication that pauses (a game freeze stops all of
// them at once) only SUSPEND it: the turn so far and the hands' last vector are kept, the last output is held a moment,
// and when tracking comes back the turn the hands made meanwhile is added. Reset there instead, the wheel came back
// centred under hands still turned -- "after a freeze the wheel's position is off" (players, 2026-11-05).
class TrackedWheelSteering {
public:
    static constexpr uint64_t kStaleUs=250000;   // tracking older than this is not used
    static constexpr uint64_t kHoldUs=2000000;   // the last output is held this long through a suspension, then 0
    void Reset() { *this={}; }
    bool CurrentSampleValid() const { return m_currentSampleValid; }
    bool PairCenteredThisUpdate() const { return m_pairCenteredThisUpdate; }
    float Angle() const { return m_wheel.Angle(); }
    float HeadTiltDegrees() const { return m_tiltDegrees; }
    bool HeadTiltValid() const { return m_tiltValid; }
    float LeanMeters() const { return m_leanMeters; }
    float LeanDegrees() const { return m_leanDegrees; }
    uint64_t SampleStampUs() const { return m_stamp; }
    // No usable sample now, the grab still owned: keep everything, hold the last output a moment.
    float Suspend(uint64_t now) {
        m_currentSampleValid=false;
        m_prediction.Reset();
        if(m_headTilt){m_tiltValid=false;return 0;} // missing head tracking must not hold a turn
        return m_fresh && now>=m_fresh && now-m_fresh<=kHoldUs ? m_last:0;
    }
    float Update(uint8_t held,const WheelTrackingFrame& frame,uint64_t now,
                 float radius,float limit,float dead,float predictionMs=0,
                 bool handlebar=false,float bodyLean=0, bool headTilt=false,float tiltLock=25,float tiltDead=3,bool directBank=false) {
        m_currentSampleValid=false;m_pairCenteredThisUpdate=false;
        held&=3;
        if(!held) {Reset();return 0;}
        const bool useHeadTilt=handlebar && headTilt;
        const bool useDirectBank=handlebar && directBank;
        if(m_headTilt!=useHeadTilt || m_directBank!=useDirectBank)Reset();
        const uint8_t tracked=held&frame.validHands;
        // A temporarily untracked hand still owns its grip. Do not turn a
        // two-hand bar into a new one-hand pivot during that interruption.
        if(useDirectBank && held==3 && tracked!=3)return Suspend(now);
        const auto& q=frame.head.orientation;
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        if(!tracked || !frame.headValid || !frame.origin || !frame.sequence || !frame.stampUs ||
           now<frame.stampUs || now-frame.stampUs>kStaleUs || !Finite(frame.head.position) ||
           !std::isfinite(norm) || norm<.5f || norm>1.5f)return Suspend(now);
        if(m_origin && m_origin!=frame.origin)Reset();
        else if(m_sequence!=frame.sequence && m_stamp && frame.stampUs<=m_stamp)return Suspend(now);
        m_origin=frame.origin;
        m_headTilt=useHeadTilt;
        m_directBank=useDirectBank;
        const float scale=1/std::sqrt(norm);
        const XrQuaternionf rotation{q.x*scale,q.y*scale,q.z*scale,q.w*scale};
        // The first hand can move while the second finds its grip. Establish
        // the riding neutral once BOTH grips are owned, not from that approach.
        // Use the physical bar span for the lean axes: looking aside at grab
        // must not turn a later forward/back body movement into lateral input.
        if(useDirectBank && tracked==3 && !m_pairCentered) {
            for(const auto& h:frame.hands)
                if(!Finite(h) || h.x*h.x+h.y*h.y+h.z*h.z>4)return Suspend(now);
            const auto r=RotateVector(rotation,frame.hands[0]);
            const auto l=RotateVector(rotation,frame.hands[1]);
            const float x=r.x-l.x,z=r.z-l.z,length=std::hypot(x,z);
            if(!std::isfinite(length)||length<.08f)return Suspend(now);
            m_bankRightX=x/length;m_bankRightZ=z/length;m_bankBasisValid=true;
            m_pairCentered=true;m_pairCenteredThisUpdate=true;
            m_wheel.Reset();m_prediction.Reset();m_leanValid=false;m_tiltNeutralValid=false;
        }
        if(useHeadTilt) {
            const auto right=RotateVector(rotation,XrVector3f{1,0,0});
            const auto up=RotateVector(rotation,XrVector3f{0,1,0});
            // Head roll relative to gravity: yaw cancels, and normal pitch
            // multiplies both terms equally. Near vertical, roll is ambiguous;
            // disable the tilt contribution without disabling handlebar steering.
            m_tiltValid=up.y>0 && std::hypot(right.y,up.y)>.258819045f;
            m_tiltShare=0;m_tiltDegrees=0;
            if(m_tiltValid) {
                const float roll=std::atan2(-right.y,up.y)*57.295779513f; // right tilt positive
                if(!m_tiltNeutralValid){m_tiltNeutral=roll;m_tiltNeutralValid=true;}
                m_tiltDegrees=std::remainder(roll-m_tiltNeutral,360.f);
                m_tiltShare=BikeTiltOutput(m_tiltDegrees,tiltLock,tiltDead);
            }
            // Tilt replaces the body-translation term, never the handlebars.
            // It is steering assistance, not an independent bike-bank axis.
        }
        const float horizon=WheelPrediction::ClampMs(predictionMs);
        if(m_held!=tracked || horizon!=m_horizon){m_prediction.Reset();m_horizon=horizon;}
        if(!m_pairCenteredThisUpdate && m_sequence==frame.sequence && m_held==tracked){m_currentSampleValid=true;return Keep(now,Output(now,limit,dead));}
        if(useDirectBank && !m_bankBasisValid) {
            const auto forward=RotateVector(rotation,XrVector3f{0,0,-1});
            const float horizontal=std::hypot(forward.x,forward.z);
            if(horizontal<.258819045f)return Suspend(now);
            m_bankRightX=-forward.z/horizontal;m_bankRightZ=forward.x/horizontal;m_bankBasisValid=true;
        }
        WheelPoint points[2]{};
        for(int i=0;i<2;++i)if(tracked&(1<<i)) {
            const auto local=frame.hands[i];
            if(!Finite(local) || local.x*local.x+local.y*local.y+local.z*local.z>4)return Suspend(now);
            const auto world=RotateVector(rotation,local);
            // Physical recenter-space right/up, independent of the character,
            // game camera, car animation and head-relative controller offsets.
            // A motorcycle's bars turn about the vertical, so the same turn is measured seen from above: right/forward
            // (OpenXR -Z is forward). Right hand pushed forward, left pulled back is a turn left, as on a real bike.
            const float x=frame.head.position.x+world.x,z=frame.head.position.z+world.z;
            points[i]=useDirectBank ? WheelPoint{x*m_bankRightX+z*m_bankRightZ,x*m_bankRightZ-z*m_bankRightX}
                : WheelPoint{x,handlebar ? -z:frame.head.position.y+world.y};
        }
        const float sampleDt=m_stamp && frame.stampUs>=m_stamp ?
            std::min(float(frame.stampUs-m_stamp)*1e-6f,.05f):0.f;
        m_sequence=frame.sequence;m_stamp=frame.stampUs;m_held=tracked;
        // THE BODY LEANING INTO A BIKE'S TURN, measured from where it was at the grab. At the neck and not the headset:
        // the eyes swing round it, and looking 60 degrees aside moved the headset about 9 cm sideways.
        m_leanDegrees=0;m_leanMeters=0;
        if(handlebar && !useHeadTilt) {
            const auto offset=RotateVector(rotation,XrVector3f{0,-kNeckBelowM,kNeckBehindM});
            const float neck=useDirectBank ? (frame.head.position.x+offset.x)*m_bankRightX+(frame.head.position.z+offset.z)*m_bankRightZ
                                          : frame.head.position.x+offset.x;
            if(!m_leanValid){m_leanNeck=neck;m_leanValid=true;}
            m_leanMeters=neck-m_leanNeck;
            m_leanDegrees=BikeLeanDegrees(m_leanMeters,bodyLean,WheelFullLockDegrees(limit));
        }
        m_wheel.Update(tracked,points[0],points[1],radius,limit,dead,handlebar,useDirectBank,sampleDt);
        m_currentSampleValid=true;
        if(!m_horizon)return Keep(now,CombinedOutput(Angle(),limit,dead));
        m_prediction.Push(Angle(),frame.stampUs);
        return Keep(now,Output(now,limit,dead));
    }
private:
    static bool Finite(XrVector3f p) {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    float Keep(uint64_t now,float out) {m_fresh=now;m_last=out;return out;}
    WheelSteering m_wheel;
    WheelPrediction m_prediction;
    float m_horizon{},m_last{};
    static constexpr float kNeckBelowM=.10f,kNeckBehindM=.10f; // estimated, from between the eyes
    float m_leanDegrees{},m_leanNeck{},m_leanMeters{};
    bool m_leanValid{},m_pairCentered{},m_pairCenteredThisUpdate{};
    bool m_headTilt{},m_tiltNeutralValid{},m_tiltValid{},m_directBank{},m_currentSampleValid{},m_bankBasisValid{};
    float m_bankRightX{},m_bankRightZ{};
    float m_tiltNeutral{},m_tiltDegrees{},m_tiltShare{};
    float CombinedOutput(float bars,float limit,float dead) const {
        if(m_directBank)return WheelSteering::Output(bars,limit,dead); // lean goes to the native bank target instead
        const float steer=WheelSteering::Output(bars+m_leanDegrees,limit,dead);
        return m_headTilt ? std::clamp(steer+m_tiltShare,-1.f,1.f):steer;
    }
    float Output(uint64_t now,float limit,float dead) const {
        return CombinedOutput(m_prediction.Angle(Angle(),now,m_horizon,limit,dead),limit,dead);
    }
    uint64_t m_sequence{},m_origin{},m_stamp{},m_fresh{};
    uint8_t m_held{};
};
}
