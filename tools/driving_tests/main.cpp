#include "Camera/BikeSeat.hpp"
#include "Anim/WheelSteering.hpp"
#include "Anim/BikeBankPolicy.hpp"
#include "Anim/BikeStunts.hpp"
#include "Anim/BikeWheelieHold.hpp"
#include "Anim/TrackedWheelSteering.hpp"
#include <cstdlib>
#include <bit>
#include <iostream>
#include <limits>
#include <string>
using namespace cvr::anim;
namespace {
constexpr float pi=3.14159265358979323846f,radius=.19f;
void Check(bool pass,const char* text) { if(!pass){std::cerr<<text<<'\n';std::exit(1);} }
void Near(float x,float y,const char* text) { Check(std::abs(x-y)<.002f,text); }
WheelPoint Rim(float degrees,int side=1,WheelPoint center={}) {
    const float a=-degrees*pi/180;
    return {center.x+side*radius*std::cos(a),center.y+side*radius*std::sin(a)};
}
float Tick(WheelSteering& wheel,int mask,WheelPoint right,WheelPoint left,float max=90,float dead=1.5f) {
    const float value=wheel.Update(uint8_t(mask),right,left,radius,max,dead);
    Check(std::isfinite(value)&&std::abs(value)<=1,"invalid output");return value;
}
void GrabOffset() {
    // Recorded stationary controller targets were below/asymmetric to the
    // animated rim. Neither hand may begin with the old absolute angle offset.
    for(int hand:{1,2}) {
        WheelSteering wheel;WheelPoint right{.178f,.40766f},left{-.24826f,.39796f};
        Near(Tick(wheel,hand,right,left),0,"grab started with steering offset");
        (hand==1 ? right:left).y-=.05f;
        const float out=Tick(wheel,hand,right,left);
        Check(hand==1 ? out>.1f:out<-.1f,"lowering a single hand did not steer");
    }
}
void OneHand() {
    for(int hand:{1,2})for(int direction:{-1,1}) {
        WheelSteering wheel;
        const WheelPoint center{.2f,-.1f};
        Tick(wheel,hand,Rim(0,1,center),Rim(0,-1,center));
        for(int angle=1;angle<=85;++angle) {
            Tick(wheel,hand,Rim(float(direction*angle),1,center),Rim(float(direction*angle),-1,center));
            Near(wheel.Angle(),float(direction*angle),"one-handed angle is not proportional or reverses");
        }
        for(int angle=84;angle>=0;--angle)Tick(wheel,hand,Rim(float(direction*angle),1,center),Rim(float(direction*angle),-1,center));
        Near(wheel.Angle(),0,"single hand did not return to center");
    }
}
void TwoHands() {
    WheelSteering wheel;Tick(wheel,3,Rim(0),Rim(0,-1));
    for(int i=0;i<=120;++i) {
        const float angle=60*std::sin(i*pi/120);
        const WheelPoint translation{.15f*std::sin(i*.05f),.10f*std::cos(i*.08f)};
        Tick(wheel,3,Rim(angle,1,translation),Rim(angle,-1,translation));
        Near(wheel.Angle(),angle,"common hand translation changed two-hand steering");
    }
}
void Handoff() {
    WheelSteering wheel;Tick(wheel,3,Rim(0),Rim(0,-1));
    for(int i=1;i<=40;++i)Tick(wheel,3,Rim(float(i)),Rim(float(i),-1));
    for(int mask:{1,3,2,3}) {
        Tick(wheel,mask,Rim(40),Rim(40,-1));
        Near(wheel.Angle(),40,"adding/releasing a hand changed the turn");
    }
    Tick(wheel,2,Rim(40),Rim(40,-1));
    Tick(wheel,2,Rim(39),Rim(39,-1));
    Near(wheel.Angle(),39,"remaining hand did not react on its next sample");
}
void LockReversal() {
    for(int mask:{1,2,3})for(int direction:{-1,1}) {
        WheelSteering wheel;Tick(wheel,mask,Rim(0),Rim(0,-1));
        for(int angle=1;angle<=210;++angle)Tick(wheel,mask,Rim(float(direction*angle)),Rim(float(direction*angle),-1));
        Near(wheel.Angle(),float(direction*210),"physical travel was discarded beyond full lock");
        for(int angle=209;angle>=0;--angle) {
            const float out=Tick(wheel,mask,Rim(float(direction*angle)),Rim(float(direction*angle),-1));
            Near(wheel.Angle(),float(direction*angle),"overtravel shifted the grab reference");
            Check(out*direction>=0,"return from full lock counter-steered before neutral");
        }
        Near(wheel.Angle(),0,"return after overtravel lost neutral");
        const float correction=Tick(wheel,mask,Rim(float(-direction*4)),Rim(float(-direction*4),-1));
        Check(correction*direction<0,"micro-correction after neutral did not change direction");
    }
}
void TrackingLoss() {
    WheelSteering wheel;Tick(wheel,1,Rim(0),{});Tick(wheel,1,Rim(30),{});
    Near(Tick(wheel,0,{},{}),0,"lost tracking held steering");
    Near(Tick(wheel,1,Rim(30),{}),0,"regrabbing revived old steering");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    Near(Tick(wheel,1,{nan,0},{}),0,"invalid tracking retained steering");
    Tick(wheel,3,Rim(0),Rim(0,-1));Tick(wheel,3,Rim(20),Rim(20,-1));
    Tick(wheel,3,{0,0},{0,0});Near(wheel.Angle(),20,"collapsed span changed angle");
    Tick(wheel,3,Rim(-20),Rim(-20,-1));Near(wheel.Angle(),20,"span recovery jumped");
}
void RateMatrix() {
    for(int hz:{30,45,90,120})for(int mask:{1,2,3})for(float duration:{.25f,.6f,1.5f}) {
        WheelSteering wheel;Tick(wheel,mask,Rim(0),Rim(0,-1));
        const int frames=int(hz*duration);
        for(int i=1;i<=frames;++i) {
            const float angle=75*float(i)/frames;
            Tick(wheel,mask,Rim(angle),Rim(angle,-1));
            Near(wheel.Angle(),angle,"cadence changed steering response");
        }
    }
    WheelSteering wheel;Tick(wheel,3,Rim(0),Rim(0,-1));
    for(int i=0;i<1500;++i) {
        const float angle=.3f*std::sin(float(i)*1.3f);
        Near(Tick(wheel,3,Rim(angle),Rim(angle,-1)),0,"stationary jitter escaped deadzone");
    }
}
void Deadzone() {
    Near(WheelSteering::Output(0,90,0),0,"zero input not neutral");
    Near(WheelSteering::Output(1.5f,90,1.5f),0,"deadzone edge not neutral");
    Near(WheelSteering::Output(2,90,1.5f),.5f/88.5f,"micro-correction is not proportional");
    for(float limit:{30,60,90,120,180}) {
        Near(WheelSteering::Output(limit,limit,20),1,"full lock setting not honored");
        Near(WheelSteering::Output(-limit,limit,20),-1,"left full lock setting not honored");
    }
}
void ExtendedTravel() {
    Near(WheelFullLockDegrees(0),180,"unset limit did not default to half-turn");
    Near(WheelFullLockDegrees(std::numeric_limits<float>::quiet_NaN()),180,"invalid limit not sanitized");
    Near(WheelFullLockForVehicle(180,false),90,"car range changed motorcycle input");
    Near(WheelFullLockForVehicle(120,false),120,"valid legacy bike setting changed");
    // The cabin wheel's curve and its inverse: the input found for an angle stands the wheel at that angle.
    Near(CarInputForCabinAngle(0),0,"no turn is not no input");
    Near(CarInputForCabinAngle(CarCabinWheelLockDeg),1,"full lock is not full input");
    Near(CarInputForCabinAngle(400),1,"overtravel exceeded full input");
    Near(CarCabinAngleForInput(.5f),49.9625f*CarCabinGain/2,"cabin curve changed at half input");
    {
        float previous=-2;
        for(int tenth=-1625;tenth<=1625;++tenth) {
            const float degrees=tenth*.1f,n=CarInputForCabinAngle(degrees);
            Check(std::abs(CarCabinAngleForInput(n)-degrees)<.001f,"input does not stand the cabin wheel at the angle");
            Check(n>previous,"input for the cabin angle does not rise");previous=n;
        }
    }
    // A car's wheel turns one to one with the cabin wheel, whatever the setting (2026-11-04).
    for(float setting:{30.f,90.f,180.f})Near(WheelFullLockForVehicle(setting,true),CarCabinWheelLockDeg,
        "car full lock is not the cabin wheel's");
    {
        WheelSteering wheel;Tick(wheel,3,Rim(0),Rim(0,-1),CarCabinWheelLockDeg,0);
        for(int angle=1;angle<=130;++angle) {
            const float out=Tick(wheel,3,Rim(float(angle)),Rim(float(angle),-1),CarCabinWheelLockDeg,0);
            Near(out,float(angle)/CarCabinWheelLockDeg,"the cabin wheel does not follow the hands one to one");
        }
    }
    for(int mask:{1,2,3})for(int direction:{-1,1}) {
        WheelSteering wheel;
        Tick(wheel,mask,Rim(0),Rim(0,-1),180);
        for(int angle=1;angle<=230;++angle) {
            const float physical=float(direction*angle);
            const float out=Tick(wheel,mask,Rim(physical),Rim(physical,-1),180);
            Near(wheel.Angle(),physical,"visual gain leaked into physical angle");
            if(angle==90)Near(out,direction*88.5f/178.5f,"quarter turn did not give half input");
            if(angle>=180)Near(out,float(direction),"half-turn did not reach full lock");
            if(angle==150) {
                const int other=mask==1?3:1;
                Tick(wheel,other,Rim(physical),Rim(physical,-1),180);
                Near(wheel.Angle(),physical,"extended handoff changed physical angle");
                Tick(wheel,mask,Rim(physical),Rim(physical,-1),180);
            }
        }
        for(int angle=229;angle>=0;--angle) {
            const float out=Tick(wheel,mask,Rim(float(direction*angle)),Rim(float(direction*angle),-1),180);
            if(angle==0)Near(out,0,"overtravel lost neutral at extended range");
        }
    }
    WheelPrediction prediction;uint64_t stamp=1000000;
    for(int i=0;i<4;++i){stamp+=10000;prediction.Push(127.f+i,stamp);}
    const float predicted=prediction.Angle(130,stamp,8,180,1.5f);
    Check(predicted>130 && predicted<=130.5f,"prediction still assumes old full lock");
}
void GamepadRange() {
    for(float inner:{0.f,.1f,.35f,.49f})for(float outer:{.7f,.9f,1.f}) {
        for(float desired:{-1.f,-.2f,-.02f,0.f,.001f,.02f,.2f,1.f}) {
            const float pad=WheelGamepadAxis(desired,inner,outer);
            // Model the game's two endpoint deadzone. Small rotations must not
            // disappear, and neutral must never emit the nonzero inner endpoint.
            const float read=std::copysign(std::clamp((std::abs(pad)-inner)/(outer-inner),0.f,1.f),pad);
            Near(read,desired,"gamepad deadzone changed requested wheel input");
            if(desired==0)Near(pad,0,"neutral emits a nonzero pad axis");
        }
    }
}
WheelTrackingFrame Tracking(uint64_t sequence,float headTime,float angle) {
    WheelTrackingFrame frame;
    frame.headValid=true;frame.validHands=3;frame.sequence=sequence;frame.origin=1;frame.stampUs=1000000+sequence*11111;
    const float yaw=.7f*std::sin(headTime),pitch=.4f*std::cos(headTime*.7f);
    frame.head.orientation=MultiplyQuat({0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)},
                                        {std::sin(pitch*.5f),0,0,std::cos(pitch*.5f)});
    frame.head.position={.12f*std::sin(headTime),1.7f+.1f*std::cos(headTime),.08f*std::sin(headTime*.4f)};
    for(int i=0;i<2;++i) {
        const auto p=Rim(angle,i==0 ? 1:-1,{0,1.4f});
        const XrVector3f world{p.x,p.y,-.4f};
        const XrVector3f relative{world.x-frame.head.position.x,world.y-frame.head.position.y,world.z-frame.head.position.z};
        frame.hands[i]=RotateVector(ConjugateQuat(frame.head.orientation),relative);
    }
    return frame;
}
void TrackedHeadMotion() {
    for(int mask:{1,2,3}) {
        TrackedWheelSteering wheel;
        for(uint64_t i=1;i<=1000;++i) {
            const auto frame=Tracking(i,float(i)*.023f,0);
            Near(wheel.Update(uint8_t(mask),frame,frame.stampUs,radius,90,1.5f),0,
                 "head yaw/pitch/translation turned stationary physical hands");
            Near(wheel.Angle(),0,"head-relative coordinates drifted the physical neutral");
        }
    }
}
void TrackedRoundtrip() {
    for(int mask:{1,2,3})for(int direction:{-1,1}) {
        TrackedWheelSteering wheel;uint64_t sequence=0;
        for(int step=0;step<=420;++step) {
            const float angle=float(direction*(step<=210 ? step:420-step));
            const auto frame=Tracking(++sequence,step*.01f,angle);
            const float out=wheel.Update(uint8_t(mask),frame,frame.stampUs,radius,90,1.5f);
            Near(wheel.Angle(),angle,"raw tracking roundtrip changed the grab origin");
            Check(out*direction>=0,"raw tracking counter-steered on return from overtravel");
        }
        Near(wheel.Angle(),0,"physical neutral was not restored after overtravel");
    }
}
// Motorcycle bars: hands 50 cm apart, turned about the vertical by `angle` (positive = right hand back = turn right).
WheelTrackingFrame Bars(uint64_t sequence,float angle,XrVector3f shift={}) {
    WheelTrackingFrame frame;
    frame.headValid=true;frame.validHands=3;frame.sequence=sequence;frame.origin=1;frame.stampUs=1000000+sequence*11111;
    frame.head.position={0,1.6f,0};
    const float a=angle/57.295779513f;
    frame.hands[0]={.25f*std::cos(a)+shift.x,-.4f+shift.y,-.4f+.25f*std::sin(a)+shift.z};
    frame.hands[1]={-.25f*std::cos(a)+shift.x,-.4f+shift.y,-.4f-.25f*std::sin(a)+shift.z};
    return frame;
}
void Handlebar() {
    const float bars=.25f,lock=BikeBarLockDefault;
    const auto update=[&](TrackedWheelSteering& w,uint8_t mask,const WheelTrackingFrame& f) {
        return w.Update(mask,f,f.stampUs,bars,lock,0,0,true);
    };
    for(int mask:{1,2,3})for(int direction:{-1,1}) {
        TrackedWheelSteering wheel;uint64_t sequence=0;
        update(wheel,uint8_t(mask),Bars(++sequence,0));
        for(int step=1;step<=25;++step) {
            const float angle=float(direction*step);
            const float out=update(wheel,uint8_t(mask),Bars(++sequence,angle));
            Near(wheel.Angle(),angle,"push-pull did not turn the bars by its angle");
            Near(out,angle/lock,"bars did not reach full lock proportionally");
        }
        for(int step=24;step>=0;--step)update(wheel,uint8_t(mask),Bars(++sequence,float(direction*step)));
        Near(wheel.Angle(),0,"bars did not return to the grab position");
    }
    TrackedWheelSteering wheel;uint64_t sequence=0;
    update(wheel,3,Bars(++sequence,0));
    Near(update(wheel,3,Bars(++sequence,0,{.05f,.1f,-.1f})),0,"moving both hands together steered");
    // Hands twisted like a car's wheel, in the right/up plane: not a handlebar movement.
    auto twisted=Bars(++sequence,0);twisted.hands[0].y+=.1f;twisted.hands[1].y-=.1f;
    Near(update(wheel,3,twisted),0,"a wheel-style twist steered the bars");
    // Body lean: the head moves over stationary hands, so the bars themselves do not turn.
    const auto leaned=[&](uint64_t s,float x,float yawDegrees) {
        auto f=Bars(s,0);
        const float half=yawDegrees/114.59155902f;
        f.head.orientation={0,std::sin(half),0,std::cos(half)};
        const XrVector3f neck{x,1.5f,.1f};
        const auto eyes=RotateVector(f.head.orientation,XrVector3f{0,.1f,-.1f});
        f.head.position={neck.x+eyes.x,neck.y+eyes.y,neck.z+eyes.z};
        const XrVector3f world[2]={{.25f,1.2f,-.4f},{-.25f,1.2f,-.4f}};
        for(int i=0;i<2;++i)f.hands[i]=RotateVector(ConjugateQuat(f.head.orientation),
            XrVector3f{world[i].x-f.head.position.x,world[i].y-f.head.position.y,world[i].z-f.head.position.z});
        return f;
    };
    const auto lean=[&](TrackedWheelSteering& w,const WheelTrackingFrame& f,float share) {
        return w.Update(3,f,f.stampUs,bars,lock,0,0,true,share);
    };
    for(float direction:{-1.f,1.f}) {
        TrackedWheelSteering body;uint64_t s=0;
        lean(body,leaned(++s,0,0),.5f);
        Near(lean(body,leaned(++s,direction*.02f,0),.5f),0,"body lean inside the deadzone steered");
        Near(lean(body,leaned(++s,direction*.15f,0),.5f),direction*.5f,"15 cm lean was not the configured share");
        Near(body.Angle(),0,"body lean turned the bars");
        Near(lean(body,leaned(++s,direction*.5f,0),1),direction,"body lean passed full lock");
        Near(lean(body,leaned(++s,direction*.15f,0),0),0,"body lean at share 0 steered");
    }
    {
        TrackedWheelSteering body;uint64_t s=0;
        lean(body,leaned(++s,0,0),.5f);
        Near(lean(body,leaned(++s,0,60),.5f),0,"looking aside counted as leaning");
    }
    for(float direction:{-1.f,1.f}) {
        TrackedWheelSteering body;uint64_t s=0;
        lean(body,leaned(++s,0,0),.5f);
        auto tilted=leaned(++s,0,0);
        const float half=direction*30/114.59155902f;
        tilted.head.orientation={0,0,std::sin(half),std::cos(half)};
        const auto eyes=RotateVector(tilted.head.orientation,XrVector3f{0,.1f,-.1f});
        tilted.head.position={eyes.x,1.5f+eyes.y,.1f+eyes.z};
        const XrVector3f world[2]={{.25f,1.2f,-.4f},{-.25f,1.2f,-.4f}};
        for(int i=0;i<2;++i)tilted.hands[i]=RotateVector(ConjugateQuat(tilted.head.orientation),
            XrVector3f{world[i].x-tilted.head.position.x,world[i].y-tilted.head.position.y,world[i].z-tilted.head.position.z});
        Near(lean(body,tilted,.5f),0,"head roll around a stationary neck counted as body lean");
    }
    Near(WheelFullLockForVehicle(120,false),120,"legacy mount setting changed");
    Near(BikeBarLockDegrees(0),BikeBarLockDefault,"unset bar lock did not default");
    Near(BikeBarLockDegrees(5),BikeBarLockMin,"bar lock below its range");
    Near(BikeBarLockDegrees(90),BikeBarLockMax,"bar lock above its range");
}
void BikeLeanLifecycle() {
    const auto frame=[](uint64_t seq,float lean,float bars=0.f) {
        auto f=Bars(seq,bars);f.head.position.x=lean;
        // Stationary physical bars; moving the head must not move the controllers.
        for(auto& hand:f.hands)hand.x-=lean;
        return f;
    };
    for(int mask:{1,2,3})for(float direction:{-1.f,1.f})for(float prediction:{0.f,8.f}) {
        TrackedWheelSteering wheel;uint64_t seq=0;
        const auto update=[&](const WheelTrackingFrame& f,int held,float share=.5f) {
            return wheel.Update(uint8_t(held),f,f.stampUs,.25f,40,0,prediction,true,share);
        };
        update(frame(++seq,0),mask);
        auto f=frame(++seq,direction*.15f);
        Near(update(f,mask),direction*.5f,"lean did not reach live steering with one/two hands or prediction");
        Near(wheel.LeanMeters(),direction*.15f,"lean readout does not match tracked displacement");
        Near(wheel.LeanDegrees(),direction*20,"lean readout does not match steering contribution");
        Check(wheel.SampleStampUs()==f.stampUs,"readout lost tracking timestamp");
        Near(update(f,mask==3 ? 1:3),direction*.5f,"hand change lost lean centre");
        Near(update(frame(++seq,0),3),0,"returning torso to centre did not straighten");
        f=frame(++seq,direction*.15f);
        update(f,3);
        Near(wheel.Update(3,f,f.stampUs+300000,.25f,40,0,prediction,true,.5f),direction*.5f,"brief tracking pause lost lean");
        auto resumed=frame(++seq,direction*.15f);resumed.stampUs=f.stampUs+800000;
        Near(update(resumed,3),direction*.5f,"resuming tracking changed lean centre");
        auto centred=resumed;centred.origin=2;
        Near(update(centred,3),0,"recenter did not reset lean origin");
        Near(update(frame(++seq,direction*.15f),0),0,"releasing last grip retained lean");
        update(frame(++seq,direction*.15f),mask);
        Near(update(frame(++seq,direction*.30f),mask),direction*.5f,"new grab did not capture new neutral");
    }
    for(float direction:{-1.f,1.f}) {
        TrackedWheelSteering wheel;
        auto a=frame(1,0);wheel.Update(3,a,a.stampUs,.25f,40,0,0,true,.5f);
        auto b=frame(2,direction*.15f,direction*10);
        Near(wheel.Update(3,b,b.stampUs,.25f,40,0,0,true,.5f),direction*.75f,"bar and lean contributions did not add");
        auto c=frame(3,direction*.30f,direction*30);
        Near(wheel.Update(3,c,c.stampUs,.25f,40,0,0,true,1),direction,"combined steering exceeded full lock");
    }
}
void BikeHeadTilt() {
    // Keep the hands fixed in tracking space when the headset turns. The
    // runtime supplies head-relative hands, so rotate them back into that frame.
    const auto pose=[](uint64_t seq,float yaw,float pitch,float roll,float bars=0.f,XrVector3f shift=XrVector3f{}) {
        auto f=Bars(seq,bars);
        const auto q=[](int axis,float degrees) {
            const float a=degrees*pi/360;XrQuaternionf q{0,0,0,std::cos(a)};
            if(axis==0)q.x=std::sin(a);else if(axis==1)q.y=std::sin(a);else q.z=std::sin(a);
            return q;
        };
        f.head.orientation=MultiplyQuat(MultiplyQuat(q(1,yaw),q(0,pitch)),q(2,-roll));
        f.head.position.x+=shift.x;f.head.position.y+=shift.y;f.head.position.z+=shift.z;
        for(auto& hand:f.hands)hand=RotateVector(ConjugateQuat(f.head.orientation),
            XrVector3f{hand.x-shift.x,hand.y-shift.y,hand.z-shift.z});
        return f;
    };
    const auto tick=[](TrackedWheelSteering& w,int held,const WheelTrackingFrame& f,float lock=25,float dead=3) {
        return w.Update(uint8_t(held),f,f.stampUs,.25f,40,0,0,true,.5f,true,lock,dead);
    };
    for(float yaw:{-170.f,-90.f,0.f,60.f,170.f})for(float pitch:{-60.f,0.f,60.f})
    for(int held:{1,2,3})for(float dir:{-1.f,1.f}) {
        TrackedWheelSteering w;uint64_t seq=0;
        Near(tick(w,held,pose(++seq,yaw,pitch,0)),0,"tilt grab should centre");
        Near(tick(w,held,pose(++seq,yaw,pitch,dir*2)),0,"tilt deadzone failed");
        Near(tick(w,held,pose(++seq,yaw,pitch,dir*14)),dir*.5f,"tilt depends on yaw/pitch/hand");
        Near(tick(w,held,pose(++seq,yaw,pitch,dir*25)),dir,"full tilt did not saturate");
        Near(tick(w,held,pose(++seq,yaw,pitch,dir*40)),dir,"tilt exceeded full lock");
        Near(tick(w,held,pose(++seq,yaw,pitch,0)),0,"tilt did not return to centre");
        Near(tick(w,held,pose(++seq,yaw,pitch,0,dir*20)),dir*.5f,"head tilt mode lost handlebar steering");
        Near(tick(w,held,pose(++seq,yaw,pitch,dir*14,dir*20)),dir,"bars and tilt did not add");
        Near(tick(w,held,pose(++seq,yaw,pitch,-dir*14,dir*20)),0,"opposite bars and tilt did not cancel");
        Near(tick(w,held,pose(++seq,yaw,pitch,dir*25,dir*20)),dir,"combined output exceeded full lock");
    }
    TrackedWheelSteering w;uint64_t seq=0;tick(w,3,pose(++seq,0,0,0));
    Near(tick(w,3,pose(++seq,150,60,0)),0,"looking around alone steered");
    auto f=pose(++seq,150,60,14,0,{.2f,.1f,.2f});
    Near(tick(w,3,f),.5f,"body translation contributed to tilt mode");
    Near(w.Angle(),0,"stationary bars changed with head motion");
    Near(tick(w,1,pose(++seq,0,0,14)),.5f,"releasing one hand reset tilt neutral");
    Near(tick(w,2,pose(++seq,0,80,30)),0,"near vertical pose must disable tilt contribution");
    Check(!w.HeadTiltValid(),"near vertical reported valid roll");
    Near(tick(w,2,pose(++seq,0,80,30,20)),.5f,"near vertical disabled valid handlebar steering");
    Near(tick(w,2,pose(++seq,0,0,14)),.5f,"vertical guard lost neutral");
    f=pose(++seq,0,0,14);f.headValid=false;
    Near(tick(w,2,f),0,"lost head tracking held a turn");
    f=pose(++seq,0,0,14);Near(tick(w,2,f),.5f,"tracking recovery reset neutral");
    Near(w.Update(2,f,f.stampUs+300000,.25f,40,0,0,true,.5f,true),0,"stale tilt held a turn");
    auto recentered=pose(++seq,0,0,14);recentered.origin=2;
    Near(tick(w,2,recentered),0,"recenter did not set tilt neutral");
    Near(tick(w,0,pose(++seq,0,0,0)),0,"released bars kept steering");
    Near(tick(w,3,pose(++seq,0,0,5)),0,"regrab did not capture new neutral");
    f=pose(++seq,0,0,19);Near(tick(w,3,f),.5f,"new tilt neutral wrong");
    Near(tick(w,3,f,14,3),1,"live lock change ignored on same sample");
    f=pose(++seq,0,0,19);
    Near(w.Update(3,f,f.stampUs,.25f,40,0,0,true,.5f,false),0,"mode switch kept old turn");
    Near(tick(w,3,pose(++seq,0,0,19)),0,"returning to tilt did not rebase");
    Near(BikeTiltLockDegrees(std::numeric_limits<float>::quiet_NaN()),25,"NaN tilt lock");
    Near(BikeTiltDeadDegrees(std::numeric_limits<float>::infinity()),3,"invalid tilt deadzone");
    Near(BikeTiltLockDegrees(-20),10,"tilt lock clamp");
    Near(BikeTiltDeadDegrees(30),8,"tilt deadzone clamp");
}
void BikeDirectBank() {
    using namespace cvr::bikebank;
    const auto pose=[](uint64_t seq,float yaw,float roll,float bars,float lean,bool moveHands) {
        auto f=Bars(seq,bars);
        const float a=yaw*pi/360,b=-roll*pi/360;
        const XrQuaternionf heading{0,std::sin(a),0,std::cos(a)},tilt{0,0,std::sin(b),std::cos(b)};
        f.head.orientation=MultiplyQuat(heading,tilt);
        const auto shift=RotateVector(heading,XrVector3f{lean,0,0});
        f.head.position={shift.x,1.6f,shift.z};
        for(auto& hand:f.hands) {
            auto world=RotateVector(heading,hand);
            if(moveHands){world.x+=shift.x;world.z+=shift.z;}
            hand=RotateVector(ConjugateQuat(f.head.orientation),XrVector3f{world.x-shift.x,world.y,world.z-shift.z});
        }
        return f;
    };
    for(float yaw:{-170.f,-90.f,0.f,60.f,170.f})for(int held:{1,2,3})for(float dir:{-1.f,1.f}) {
        TrackedWheelSteering w;uint64_t seq=0;
        const auto tick=[&](const WheelTrackingFrame& f,bool tilt=true) {
            return w.Update(uint8_t(held),f,f.stampUs,.25f,40,0,0,true,.5f,tilt,25,3,true);
        };
        Near(tick(pose(++seq,yaw,0,0,0,false)),0,"direct bank starts neutral");
        auto f=pose(++seq,yaw,dir*13,0,.1f,false);
        Near(tick(f),0,"direct head bank leaked into steering");
        Near(RequestedDegrees(true,w.HeadTiltValid(),w.HeadTiltDegrees(),w.LeanMeters(),3,1,20),dir*10,"head bank not angular 1:1");
        f=pose(++seq,yaw,dir*13,dir*20,.1f,false);
        Near(tick(f),dir*.5f,"direct bank lost independent handlebars");
        Check(w.CurrentSampleValid(),"valid input not published");
        w.Suspend(f.stampUs+300000);Check(!w.CurrentSampleValid(),"suspended bank input valid");
        f=pose(++seq,yaw,dir*13,dir*20,.1f,false);tick(f);
        f.headValid=false;tick(f);Check(!w.CurrentSampleValid(),"tracking loss left bank valid");
        f=pose(++seq,yaw,dir*13,dir*20,.1f,false);f.origin=2;
        Near(tick(f),0,"recenter retained bar angle");Near(w.HeadTiltDegrees(),0,"recenter retained bank");
        w.Update(0,f,f.stampUs,.25f,40,0,0,true,.5f,true,25,3,true);
        Check(!w.CurrentSampleValid(),"release retained usable bank input");
        // Switching back to original mode must rebase, without an old bank becoming steering.
        f=pose(++seq,yaw,dir*13,dir*20,.1f,false);tick(f);
        Near(w.Update(uint8_t(held),f,f.stampUs,.25f,40,0,0,true,.5f,true,25,3,false),0,"bank off retained old neutral");
    }
    for(float yaw:{-170.f,-90.f,0.f,60.f,170.f})for(float dir:{-1.f,1.f})for(bool movingHands:{false,true}) {
        TrackedWheelSteering w;uint64_t seq=0;
        const auto tick=[&](const WheelTrackingFrame& f) {return w.Update(3,f,f.stampUs,.25f,40,0,0,true,.5f,false,25,3,true);};
        tick(pose(++seq,yaw,0,0,0,movingHands));
        Near(tick(pose(++seq,yaw,0,0,dir*.15f,movingHands)),0,"body bank leaked into bar steering");
        Near(w.LeanMeters(),dir*.15f,"body bank depends on room heading");
        Near(RequestedDegrees(false,true,0,w.LeanMeters(),3,1,20),dir*std::atan2(.12f,.5f)/kRadPerDegree,"body bank estimate wrong");
        Near(tick(pose(++seq,yaw,0,dir*20,dir*.15f,movingHands)),dir*.5f,"body bank lost independent bar steering");
    }
}
void BikeLateralContact() {
    BikeAttitude a{};a.stampUs=1000000;a.vehicle=42;a.wheelsValid=true;a.frontContact=a.rearContact=true;
    Check(BikeLeanGrounded(a,1000010,42),"fresh grounded contact disabled lean");
    a.frontContact=false;Check(!BikeLeanGrounded(a,1000010,42),"wheelie got ground lean steering");
    a.frontContact=true;a.rearContact=false;Check(!BikeLeanGrounded(a,1000010,42),"stoppie got ground lean steering");
    a.rearContact=true;a.scriptedTilt=true;Check(!BikeLeanGrounded(a,1000010,42),"scripted bank got extra steer");
    a.scriptedTilt=false;Check(!BikeLeanGrounded(a,1100001,42),"stale contact used");
    Check(!BikeLeanGrounded(a,999999,42),"future contact used");
    Check(!BikeLeanGrounded(a,1000010,43),"other vehicle contact used");
    a.wheelsValid=false;Check(!BikeLeanGrounded(a,1000010,42),"invalid contact used");
}
void BikeLateralCoupling() {
    using namespace cvr::bikebank;
    for(float sign:{-1.f,1.f}) {
        Near(LeanSteering(sign*20,.5f),sign*.25f,"lean alone does not turn gently");
        Near(LeanSteering(sign*45,1),sign*.35f,"lean turn exceeds cap");
        Near(LeanSteering(sign*20,0),0,"zero strength must disable lean steering");
        Near(CoordinatedTarget(sign*12*kRadPerDegree,sign*20,38)/kRadPerDegree,sign*20,"native and lean bank double counted");
        Near(CoordinatedTarget(sign*30*kRadPerDegree,sign*20,38)/kRadPerDegree,sign*30,"stronger native bank lost");
        Near(CoordinatedTarget(sign*12*kRadPerDegree,-sign*5,38)/kRadPerDegree,sign*7,"counter lean lost");
        Near(CoordinatedTarget(sign*30*kRadPerDegree,sign*45,38)/kRadPerDegree,sign*38,"native bank limit exceeded");
        Near(CoordinatedTarget(sign*12*kRadPerDegree,0,38)/kRadPerDegree,sign*12,"upright rider altered native target");
    }
    Near(LeanSteering(std::numeric_limits<float>::quiet_NaN(),.5f),0,"NaN steering");
    Near(CoordinatedTarget(.2f,20,0),.2f,"invalid native limit modified");
    Near(CoordinatedTarget(1.f,20,38),1.f,"out of bounds native bank modified");
}
void BikeLeanRamp() {
    using namespace cvr::bikebank;
    for(int hz:{30,60,90,144})for(float sign:{-1.f,1.f}) {
        LeanRamp ramp;uint64_t t=1000000;const uint64_t dt=1000000/hz;
        Near(ramp.Update(sign*20,t,1,42),0,"new grab starts with torque");
        float previous=0;
        for(int i=0;i<hz;++i) {
            t+=dt;const float value=ramp.Update(sign*20,t,1,42);
            Check(sign*value>=sign*previous && std::abs(value)<=20.001,"lean ramp overshot");
            Near(ramp.Update(sign*20,t,1,42),value,"repeat input poll advanced ramp");previous=value;
        }
        Near(previous,sign*20,"steady lean not reached");
        for(int i=0;i<hz;++i) {
            t+=dt;const float value=ramp.Update(0,t,1,42);
            Check(sign*value>=0 && std::abs(value)<=std::abs(previous)+.0001,"recovery reversed or grew");previous=value;
        }
        Near(previous,0,"recovery retained a turn");
        t+=dt;ramp.Update(sign*20,t,1,42);
        Near(ramp.Update(sign*20,t+dt,2,42),0,"recenter did not reset lean");
        Near(ramp.Update(sign*20,t+2*dt,2,43),0,"vehicle change did not reset lean");
        Near(ramp.Update(sign*20,t+1000000,2,43),0,"stale input retained lean");
    }
}
void BikePairRecovery() {
    for(int hz:{30,60,90,144})for(int single:{1,2})for(float direction:{-1.f,1.f}) {
        WheelSteering wheel;const float dt=1.f/hz;
        auto push=[&](int held,float y,float angle=0.f) {
            const float rad=-angle*pi/180;
            return wheel.Update(held,{.25f*std::cos(rad),y+.25f*std::sin(rad)},
                {-.25f*std::cos(rad),y-.25f*std::sin(rad)},.25f,40,0,true,true,dt);
        };
        push(3,0);push(3,direction*.06f);push(single,direction*.06f);push(single,0);
        const float wrong=wheel.Angle();Check(std::abs(wrong)>10,"reproduction did not exercise pivot drift");
        float last=std::abs(wrong);
        for(int i=0;i<hz;++i) {
            push(3,0);Check(wheel.Angle()*wrong>=-.0001,"pair recovery countersteered beyond centre");
            Check(std::abs(wheel.Angle())<=last+.0001,"pair recovery increased wrong angle");
            Check(last-std::abs(wheel.Angle())<=90*dt+.001,"pair recovery snapped");last=std::abs(wheel.Angle());
        }
        Near(wheel.Angle(),0,"two-hand centre permanently shifted");
        Near(push(3,0,20),.5f,"intentional steering lost after recovery");
        Near(push(3,0,0),0,"return from new turn shifted centre");
    }
}
void BikeInitialPairNeutral() {
    for(int hand:{1,2})for(float side:{-1.f,1.f}) {
        TrackedWheelSteering w;uint64_t seq=1;
        auto tick=[&](int held,WheelTrackingFrame f) {return w.Update(uint8_t(held),f,f.stampUs,.25f,40,0,0,true,.5f,false,25,3,true);};
        auto first=Bars(seq++,0,{side*.10f,0,.06f});first.head.position.x=side*.10f;
        tick(hand,first);tick(hand,Bars(seq++,0));
        Near(tick(3,Bars(seq++,0)),0,"first complete grip inherited approach-hand steering");
        Near(w.LeanMeters(),0,"first complete grip inherited approach-body lean");
        Near(tick(3,Bars(seq++,20)),.5f,"steering lost after complete grip calibration");
        tick(hand,Bars(seq++,20));
        Near(tick(3,Bars(seq++,20)),.5f,"later rejoining hand reset deliberate turn");
        Near(tick(3,Bars(seq++,0)),0,"paired neutral shifted after regrip");
    }
}
void BikeLookAsideNeutral() {
    for(float yaw:{-75.f,-45.f,0.f,45.f,75.f}) {
        TrackedWheelSteering w;uint64_t seq=0;
        auto pose=[&](float look,float sideways,float foreaft) {
            auto f=Bars(++seq,0);
            const float a=look*pi/360;f.head.orientation={0,std::sin(a),0,std::cos(a)};
            const auto offset=RotateVector(f.head.orientation,XrVector3f{0,-.10f,.10f});
            f.head.position={sideways-offset.x,1.5f-offset.y,.1f+foreaft-offset.z};
            for(int i=0;i<2;++i) {
                const XrVector3f world{i==0?.25f:-.25f,1.2f,-.4f};
                f.hands[i]=RotateVector(ConjugateQuat(f.head.orientation),XrVector3f{
                    world.x-f.head.position.x,world.y-f.head.position.y,world.z-f.head.position.z});
            }
            return f;
        };
        auto tick=[&](WheelTrackingFrame f){return w.Update(3,f,f.stampUs,.25f,40,0,0,true,.5f,false,25,3,true);};
        tick(pose(yaw,0,0));
        for(float forward:{-.15f,0.f,.15f}) {
            Near(tick(pose(0,0,forward)),0,"looking aside at grab changed handlebar neutral");
            Near(w.LeanMeters(),0,"forward lean became lateral turn after looking aside at grab");
        }
        for(float side:{-.15f,.15f}) {
            tick(pose(-yaw,side,0));Near(w.LeanMeters(),side,"lateral lean scaled with initial head yaw");
        }
    }
}
void BikePairTracking() {
    TrackedWheelSteering wheel;uint64_t seq=1;
    auto tick=[&](WheelTrackingFrame f){return wheel.Update(3,f,f.stampUs,.25f,40,0,0,true,.5f,false,25,3,true);};
    tick(Bars(seq++,0));Near(tick(Bars(seq++,20)),.5f,"initial bar turn");
    auto missing=Bars(seq++,20);missing.validHands=1;tick(missing);
    Check(!wheel.CurrentSampleValid(),"one missing hand interpreted as new grip");
    Near(tick(Bars(seq++,0)),0,"tracking gap retained steering offset");
    Near(tick(Bars(seq++,20)),.5f,"tracking recovery lost steering");
}

void BikeBankPolicy() {
    using namespace cvr::bikebank;
    for(float dir:{-1.f,1.f}) {
        Near(RequestedDegrees(true,true,dir*2,0,3,1,20),0,"head deadzone");
        Near(RequestedDegrees(true,false,dir*20,0,3,1,20),0,"invalid tilt used");
        Near(RequestedDegrees(true,true,dir*30,0,3,1,20),dir*20,"added angle cap");
        Near(RequestedDegrees(true,true,dir*13,0,3,.5f,20),dir*5,"head bank gain");
        Near(RequestedDegrees(false,true,0,dir*.02f,3,1,20),0,"body deadzone");
        Near(AddToNative(dir*10*kRadPerDegree,dir*15,45)/kRadPerDegree,dir*25,"bank not additive");
        Near(AddToNative(dir*40*kRadPerDegree,dir*20,45)/kRadPerDegree,dir*45,"native tilt cap");
        Near(AddToNative(dir*40*kRadPerDegree,-dir*20,45)/kRadPerDegree,dir*20,"opposing lean wrong");
        Near(AddToNative(dir*60*kRadPerDegree,dir*20,45)/kRadPerDegree,dir*60,"changed out-of-range native bank");
    }
    for(float native:{-1.f,-.5f,-0.f,0.f,.5f,1.f}) {
        Check(std::bit_cast<uint32_t>(native)==std::bit_cast<uint32_t>(AddToNative(native,0,45)),"neutral changed native bits");
        Near(AddToNative(native,10,0),native,"invalid native limit applied");
        Near(AddToNative(native,std::numeric_limits<float>::quiet_NaN(),45),native,"NaN bank applied");
    }
    Check(!Fresh(100,0)&&!Fresh(100,101)&&Fresh(100100,100)&&!Fresh(100101,100),"freshness boundary");
    for(int hz:{30,45,90,120}) {
        const uint64_t stamp=1000000,interval=1000000/hz;
        Check(Fresh(stamp+interval,stamp),"valid frame frequency dropped");
        Check(!Fresh(stamp+300000,stamp),"stale frame accepted");
    }
}

WheelTrackingFrame StuntPose(uint64_t seq,float facing,float yaw,float pitch,float roll,float forward,float sideways=0) {
    auto f=Bars(seq,0);
    const auto quat=[](int axis,float deg) {
        const float a=deg*pi/360;XrQuaternionf q{0,0,0,std::cos(a)};
        if(axis==0)q.x=std::sin(a);else if(axis==1)q.y=std::sin(a);else q.z=std::sin(a);return q;
    };
    const auto basis=quat(1,facing);
    f.head.orientation=MultiplyQuat(MultiplyQuat(quat(1,facing+yaw),quat(0,pitch)),quat(2,roll));
    // Head turns about the same physical neck; translations are true body motion.
    const auto eyes=RotateVector(f.head.orientation,XrVector3f{0,.1f,-.1f});
    const auto shift=RotateVector(basis,XrVector3f{sideways,0,-forward});
    f.head.position={eyes.x+shift.x,1.5f+eyes.y,eyes.z+shift.z};
    return f;
}
void BikeStuntMotion() {
    for(float facing:{-170.f,-90.f,0.f,60.f,170.f})for(int held:{1,2,3})for(float dir:{-1.f,1.f}) {
        TrackedBikeStunts s;uint64_t seq=0;
        const auto tick=[&](WheelTrackingFrame f){s.Update(uint8_t(held),f,f.stampUs,123);return s.Axis(f.stampUs,.15f,.03f);};
        Near(tick(StuntPose(++seq,facing,0,0,0,0)),0,"stunt starts neutral");
        Near(tick(StuntPose(++seq,facing,0,0,0,dir*.02f)),0,"posture deadzone failed");
        Near(tick(StuntPose(++seq,facing,0,0,0,dir*.09f)),dir*.5f,"native lean direction or midpoint wrong");
        Near(tick(StuntPose(++seq,facing,0,0,0,dir*.15f)),dir,"full physical lean");
        Near(tick(StuntPose(++seq,facing,0,0,0,dir*.35f)),dir,"native lean exceeds full input");
        Near(tick(StuntPose(++seq,facing,0,0,0,0)),0,"return upright not neutral");
        Near(tick(StuntPose(++seq,facing,0,0,0,0,dir*.2f)),0,"sideways bank added wheelie/stoppie");
    }
    // No nod/looking trigger when the tracked eyes rotate about the neck.
    for(float facing:{-130.f,0.f,120.f}) {
        TrackedBikeStunts s;uint64_t seq=0;auto f=StuntPose(++seq,facing,0,0,0,0);s.Update(3,f,f.stampUs,123);
        for(float yaw:{-90.f,-45.f,0.f,90.f})for(float pitch:{-60.f,0.f,60.f})for(float roll:{-25.f,0.f,25.f}) {
            f=StuntPose(++seq,facing,yaw,pitch,roll,0);s.Update(3,f,f.stampUs,123);
            Near(s.Axis(f.stampUs,.15f,.03f),0,"head rotation created fore/aft input");
            f=StuntPose(++seq,facing,yaw,pitch,roll,.09f);s.Update(3,f,f.stampUs,123);
            Near(s.Axis(f.stampUs,.15f,.03f),.5f,"looking changed physical lean heading");
        }
    }
}
void BikeStuntLifecycle() {
    TrackedBikeStunts s;uint64_t seq=0;
    auto f=StuntPose(++seq,0,0,0,0,0);s.Update(3,f,f.stampUs,123);
    f=StuntPose(++seq,0,0,0,0,-.15f);s.Update(3,f,f.stampUs,123);
    Near(s.Axis(f.stampUs,.15f,.03f),-1,"wheelie setup");
    s.Update(1,f,f.stampUs,123);Near(s.Axis(f.stampUs,.15f,.03f),-1,"one-hand change reset neutral");
    Near(s.Axis(f.stampUs+100001,.15f,.03f),0,"stale input held wheelie");
    Check(!s.Fresh(f.stampUs-1),"future input accepted");
    f.headValid=false;s.Update(1,f,f.stampUs,123);Near(s.Axis(f.stampUs,.15f,.03f),0,"lost head held wheelie");
    f=StuntPose(++seq,0,0,0,0,-.15f);s.Update(1,f,f.stampUs,123);Near(s.Axis(f.stampUs,.15f,.03f),-1,"tracking recovery reset neutral");
    auto backwards=f;backwards.sequence++;backwards.stampUs--;s.Update(1,backwards,f.stampUs,123);Check(!s.Fresh(f.stampUs),"out-of-order input used");
    f=StuntPose(++seq,0,0,0,0,-.15f);s.Update(1,f,f.stampUs,123);s.Suspend();Near(s.Axis(f.stampUs,.15f,.03f),0,"suspension held lean");
    f.origin=2;s.Update(1,f,f.stampUs,123);Near(s.Axis(f.stampUs,.15f,.03f),0,"recenter retained lean");
    f=StuntPose(++seq,0,0,0,0,0);s.Update(1,f,f.stampUs,456);Near(s.Axis(f.stampUs,.15f,.03f),0,"new bike retained old reference");
    f=StuntPose(++seq,0,0,0,0,.15f);s.Update(1,f,f.stampUs,456);Near(s.Axis(f.stampUs,.15f,.03f),1,"new bike setup");
    s.Update(0,f,f.stampUs,456);Near(s.Axis(f.stampUs,.15f,.03f),0,"grip release retained stunt");
    s.Update(1,f,f.stampUs,456);Near(s.Axis(f.stampUs,.15f,.03f),0,"regrab did not capture current position");
    f=StuntPose(++seq,0,0,0,0,1);s.Update(1,f,f.stampUs,456);Check(!s.Fresh(f.stampUs),"tracking jump counted as stunt");
    f=StuntPose(++seq,0,0,0,0,.15f);f.head.position.x=std::numeric_limits<float>::quiet_NaN();s.Update(1,f,f.stampUs,456);Check(!s.Fresh(f.stampUs),"NaN position accepted");
    s.Reset();f=StuntPose(++seq,0,0,90,0,0);s.Update(1,f,f.stampUs,456);Check(!s.Fresh(f.stampUs),"grab facing vertically has no heading");
    f=StuntPose(++seq,0,0,0,0,0);s.Update(1,f,f.stampUs,0);Check(!s.Fresh(f.stampUs),"unmounted input accepted");
}
void BikeStuntNativeInput() {
    for(float dir:{-1.f,1.f}) {
        Near(BikeStuntMerge(0,dir*.8f),dir*.8f,"physical native input missing");
        Near(BikeStuntMerge(-dir*.4f,dir*.8f),-dir*.4f,"stick cannot counter physical lean");
        Near(BikeStuntMerge(dir*.3f,dir*.8f),dir*.3f,"explicit stick did not override");
        Near(BikeStuntMerge(dir*.3f,0),dir*.3f,"no physical lean changed stick");
        Near(BikeStuntMerge(0,dir*2),dir,"unbounded merged input");
        for(float inner:{0.f,.2f,.35f})for(float outer:{.8f,.9f,1.f}) {
            const float raw=WheelGamepadAxis(dir*.5f,inner,outer);
            Near((std::abs(raw)-inner)/(outer-inner),.5f,"native deadzone changes intended lean");
        }
    }
    Near(BikeStuntFullMeters(std::numeric_limits<float>::quiet_NaN()),.15f,"invalid full travel");
    Near(BikeStuntAxis(.08f,.08f,.08f),1,"deadzone can reach full travel");
    Near(BikeStuntAxis(std::numeric_limits<float>::quiet_NaN(),.15f,.03f),0,"NaN lean used");
    for(int hz:{30,45,60,90,120}) {
        TrackedBikeStunts s;auto f=StuntPose(1,0,0,0,0,0);s.Update(3,f,f.stampUs,1);
        const auto start=f.stampUs;
        for(int i=1;i<=hz;i++) {
            f=StuntPose(i+1,0,0,0,0,.15f*i/hz);f.stampUs=start+uint64_t(1000000.*i/hz);s.Update(3,f,f.stampUs,1);
        }
        Near(s.Axis(f.stampUs,.15f,.03f),1,"lean depends on frame rate");
    }
}

void TrackedLifetime() {
    TrackedWheelSteering wheel;auto frame=Tracking(1,0,0);
    wheel.Update(1,frame,frame.stampUs,radius,90,1.5f);
    frame=Tracking(2,.1f,30);const float out=wheel.Update(1,frame,frame.stampUs,radius,90,1.5f);
    Near(wheel.Angle(),30,"tracking setup failed");
    wheel.Update(3,frame,frame.stampUs,radius,90,1.5f);
    Near(wheel.Angle(),30,"same-sequence grip handoff reset the turn");
    // Stale tracking: the input is held a moment, then let go -- the turn itself is kept for when tracking is back.
    Near(wheel.Update(3,frame,frame.stampUs+250001,radius,90,1.5f),out,"stale tracking dropped the input at once");
    Near(wheel.Update(3,frame,frame.stampUs+TrackedWheelSteering::kHoldUs+1,radius,90,1.5f),0,"stale tracking held the input for good");
    Near(wheel.Angle(),30,"stale tracking lost the turn");
    frame=Tracking(3,.2f,0);wheel.Update(1,frame,frame.stampUs,radius,90,1.5f);
    frame=Tracking(4,.2f,30);wheel.Update(1,frame,frame.stampUs,radius,90,1.5f);
    frame.origin=2;Near(wheel.Update(1,frame,frame.stampUs,radius,90,1.5f),0,"recenter retained old tracking-space pivot");
    frame=Tracking(5,.3f,0);frame.headValid=false;
    Near(wheel.Update(1,frame,frame.stampUs,radius,90,1.5f),0,"lost head tracking retained steering");
}
// The game freezes with the wheel turned: no fresh sample for 0.8 s while the hands turn on, then tracking is back.
void TrackedFreeze() {
    for(int mask:{1,2,3}) {
        TrackedWheelSteering wheel;uint64_t sequence=0;
        for(int a=0;a<=40;++a){const auto f=Tracking(++sequence,a*.01f,float(a));wheel.Update(uint8_t(mask),f,f.stampUs,radius,90,1.5f);}
        Near(wheel.Angle(),40,"freeze setup failed");
        const uint64_t frozen=1000000+sequence*11111;
        Check(wheel.Suspend(frozen+800000)>0,"a freeze dropped the held input");
        auto f=Tracking(++sequence,.5f,100);f.stampUs=frozen+800000;
        wheel.Update(uint8_t(mask),f,f.stampUs,radius,90,1.5f);
        Near(wheel.Angle(),100,"the turn the hands made during a freeze was lost");
        wheel.Update(uint8_t(mask),f,f.stampUs+600000,radius,90,1.5f);
        Near(wheel.Angle(),100,"a sample polled late reset the wheel");
        for(int a=99;a>=0;--a) {
            auto g=Tracking(++sequence,.6f+a*.001f,float(a));g.stampUs=f.stampUs+700000+uint64_t(100-a)*11111;
            wheel.Update(uint8_t(mask),g,g.stampUs,radius,90,1.5f);
        }
        Near(wheel.Angle(),0,"after a freeze the wheel did not come back to neutral");
        // a fast two-handed turn across the gap, past the old 0.5 m cap on the vector's change (82 degrees)
        auto h=Tracking(++sequence,.7f,120);h.stampUs=f.stampUs+700000+200*11111;
        wheel.Update(uint8_t(mask),h,h.stampUs,radius,180,1.5f);
        Near(wheel.Angle(),120,"a 120-degree turn across a gap was dropped");
    }
}
void PredictionConstant() {
    for(int hz:{30,45,90,120})for(float speed:{-180.f,-90.f,-45.f,-10.f,10.f,45.f,90.f,180.f}) {
        WheelPrediction prediction;const uint64_t step=1000000/hz;
        for(int i=0;i<4;++i) {
            const float angle=std::copysign(20.f,speed)+speed*float(i*step)*1e-6f;
            const uint64_t stamp=1000000+i*step;prediction.Push(angle,stamp);
            const float lead=prediction.Angle(angle,stamp,8,90,1.5f)-angle;
            if(i<3)Near(lead,0,"prediction started without consistent history");
            else Near(lead,std::clamp(speed*.008f,-.5f,.5f),"constant turn prediction outside expected bound");
        }
    }
}
void PredictionStopReverse() {
    WheelPrediction prediction;uint64_t stamp=1000000;float angle=20;
    for(int i=0;i<8;++i){angle+=.5f;stamp+=10000;prediction.Push(angle,stamp);}
    Check(prediction.Angle(angle,stamp,8,90,1.5f)>angle,"moving setup has no lead");
    stamp+=10000;prediction.Push(angle,stamp);
    Near(prediction.Angle(angle,stamp,8,90,1.5f),angle,"stop retained a prediction tail");
    for(int i=0;i<6;++i){angle+=.5f;stamp+=10000;prediction.Push(angle,stamp);}
    angle-=.5f;stamp+=10000;prediction.Push(angle,stamp);
    Near(prediction.Angle(angle,stamp,8,90,1.5f),angle,"reversal used outgoing velocity");
    for(int i=0;i<4;++i){angle-=.5f;stamp+=10000;prediction.Push(angle,stamp);}
    Check(prediction.Angle(angle,stamp,8,90,1.5f)<angle,"confirmed reverse motion did not predict");
    prediction.Reset();
    for(int i=0;i<20;++i){stamp+=10000;prediction.Push(0,stamp);}
    for(int i=0;i<20;++i){stamp+=10000;prediction.Push(4,stamp);
        Near(prediction.Angle(4,stamp,8,90,1.5f),4,"a step from rest gained unconfirmed overshoot");}
}
void PredictionBoundaries() {
    for(float angle:{-210.f,-90.f,-89.9f,-3.f,-1.51f,-1.5f,-.1f,0.f,.1f,1.5f,1.51f,3.f,89.9f,90.f,210.f})
    for(int direction:{-1,1}) {
        WheelPrediction prediction;uint64_t stamp=1000000;
        for(int i=0;i<4;++i){stamp+=10000;prediction.Push(angle-float(direction*(3-i)),stamp);}
        const float result=prediction.Angle(angle,stamp,8,90,1.5f);
        Check(std::abs(result-angle)<=.50001f,"prediction exceeded half a degree");
        if(std::abs(angle)<=1.5f || std::abs(angle)>=90)Near(result,angle,"prediction altered neutral or full-lock saturation");
        else Check(result*angle>0 && std::abs(result)>=1.5f && std::abs(result)<=90,
                   "prediction crossed neutral/deadzone/full lock");
    }
}
void PredictionFreshness() {
    WheelPrediction prediction;uint64_t stamp=1000000;
    for(int i=0;i<4;++i){stamp+=10000;prediction.Push(20.f+i,stamp);}
    const float first=prediction.Angle(23,stamp,8,90,1.5f);
    for(int i=1;i<=20;++i)Near(prediction.Angle(23,stamp+i*1000,8,90,1.5f),first,"polling extrapolated the same pose farther");
    Near(prediction.Angle(23,stamp+60001,8,90,1.5f),23,"stale history retained prediction");
    Near(prediction.Angle(23,stamp-1,8,90,1.5f),23,"future-dated history was accepted");
    Near(prediction.Angle(23,stamp,0,90,1.5f),23,"disabled prediction changed steering");
    Near(prediction.Angle(23,stamp,100,90,1.5f),first,"horizon exceeded its hard cap");
    prediction.Push(24,stamp+100000);Near(prediction.Angle(24,stamp+100000,8,90,1.5f),24,"publication gap reused old history");
}
void PredictionTracking() {
    for(int mask:{1,2,3})for(int direction:{-1,1}) {
        TrackedWheelSteering direct,predicted,disabled;uint64_t sequence=0;
        for(int step=0;step<=420;++step) {
            const float angle=float(direction*(step<=210 ? step:420-step));
            const auto frame=Tracking(++sequence,step*.01f,angle);
            const float reference=direct.Update(uint8_t(mask),frame,frame.stampUs,radius,90,1.5f);
            const float value=predicted.Update(uint8_t(mask),frame,frame.stampUs,radius,90,1.5f,8);
            Near(predicted.Angle(),direct.Angle(),"prediction fed back into physical angle");
            Check(value*direction>=0,"prediction counter-steered during a physical return");
            if(step==0 || step==420)Near(value,0,"predicted roundtrip missed neutral");
            Check(std::bit_cast<uint32_t>(disabled.Update(uint8_t(mask),frame,frame.stampUs,radius,90,1.5f,0))==std::bit_cast<uint32_t>(reference),
                  "disabled prediction changed the direct input bits");
        }
    }
    TrackedWheelSteering wheel;auto frame=Tracking(1,0,0);
    wheel.Update(3,frame,frame.stampUs,radius,90,1.5f,8);
    for(int i=2;i<24;++i){frame=Tracking(i,.1f*i,float(i));wheel.Update(3,frame,frame.stampUs,radius,90,1.5f,8);}
    float raw=WheelSteering::Output(wheel.Angle(),90,1.5f);
    Check(wheel.Update(3,frame,frame.stampUs,radius,90,1.5f,8)>raw,"tracked prediction setup failed");
    Near(wheel.Update(3,frame,frame.stampUs,radius,90,1.5f,0),raw,"disabling prediction retained the lead");
    Near(wheel.Update(3,frame,frame.stampUs,radius,90,1.5f,8),raw,"enabling prediction reused pre-toggle history");
    for(int i=24;i<30;++i){frame=Tracking(i,.1f*i,float(i));wheel.Update(3,frame,frame.stampUs,radius,90,1.5f,8);}
    raw=WheelSteering::Output(wheel.Angle(),90,1.5f);
    Check(wheel.Update(3,frame,frame.stampUs,radius,90,1.5f,8)>raw,"prediction did not rearm after a toggle");
    Near(wheel.Update(1,frame,frame.stampUs,radius,90,1.5f,8),raw,"grip handoff carried prediction velocity");
    frame.origin=2;Near(wheel.Update(1,frame,frame.stampUs,radius,90,1.5f,8),0,"recenter carried predicted input");
}
void PredictionJitter() {
    for(float centre:{0.f,20.f,-20.f}) {
        WheelPrediction prediction;float maximum=0;
        for(int i=0;i<1500;++i) {
            const float angle=centre+.3f*std::sin(i*1.0471f);
            const uint64_t stamp=1000000+i*11111;prediction.Push(angle,stamp);
            const float result=prediction.Angle(angle,stamp,8,90,1.5f);
            maximum=std::max(maximum,std::abs(result-angle));
            if(centre==0)Near(WheelSteering::Output(result,90,1.5f),0,"neutral jitter triggered steering");
        }
        Check(maximum<.15f,"fixed-position jitter gained excessive prediction");
    }
}
void PredictionModel() {
    // This is a numerical continuing-motion comparison, not measured hardware
    // latency: apply a fixed 44ms plant delay to the unfiltered/predicted input.
    for(int hz:{30,45,90,120}) {
        WheelPrediction prediction;double directError=0,predictedError=0;int count=0;
        const auto motion=[](double t){return float(30+15*std::sin(2*3.141592653589793*.4*t));};
        for(int i=0;i<8*hz;++i) {
            const double t=double(i)/hz;const auto stamp=uint64_t(1000000+t*1000000);
            const float angle=motion(t);prediction.Push(angle,stamp);
            const float predicted=prediction.Angle(angle,stamp,8,90,1.5f);
            if(i>hz) {
                const float target=motion(t+.044);
                directError+=(angle-target)*(angle-target);predictedError+=(predicted-target)*(predicted-target);++count;
            }
        }
        Check(predictedError<directError*.95,"conservative lead did not help continuing-motion model");
        std::cout<<hz<<"Hz modeled RMS degrees: direct="<<std::sqrt(directError/count)
                 <<" predicted="<<std::sqrt(predictedError/count)<<'\n';
    }
}
BikeAttitude Ground(uint64_t now=1000000) {
    BikeAttitude a{};a.stampUs=now;a.vehicle=42;a.valid=true;a.wheelsValid=true;
    a.frontContact=a.rearContact=true;a.contacts=2;return a;
}
void BikeContactSteering() {
    Check(BikeNativeWheelContact(-.001f)&&BikeNativeWheelContact(0.f),"native ground predicate changed");
    Check(!BikeNativeWheelContact(.001f)&&!BikeNativeWheelContact(std::numeric_limits<float>::max()),"native no-hit sentinel counted as ground");
    Check(!BikeNativeWheelContact(std::numeric_limits<float>::infinity()) &&
          !BikeNativeWheelContact(std::numeric_limits<float>::quiet_NaN()),"native unordered/no-hit predicate changed");
    auto a=Ground();const uint64_t now=a.stampUs;
    Check(!BikeFrontLifted(a,now,42),"grounded front lifted");
    a.rearContact=false;a.contacts=1;
    Check(!BikeFrontLifted(a,now,42),"stoppie disabled handlebars");
    Near(BikeAirSteering(.7f,-.2f,true,BikeFrontLifted(a,now,42)),.7f,"stoppie steering changed");
    a.rearContact=true;a.frontContact=false;
    Check(BikeFrontLifted(a,now,42),"wheelie not recognized");
    Near(BikeAirSteering(.7f,-.2f,true,true),0,"direct-bank wheelie kept handlebars");
    Near(BikeAirSteering(.7f,-.2f,false,true),-.2f,"legacy wheelie lost sideways lean");
    a.rearContact=false;a.contacts=0;
    Check(BikeFrontLifted(a,now,42),"airborne front kept bar steering");
    Check(!BikeFrontLifted(a,now+100001,42),"stale wheel contact used");
    Check(!BikeFrontLifted(a,now,43),"foreign bike wheel contact used");
    a.wheelsValid=false;Check(!BikeFrontLifted(a,now,42),"unsupported wheels guessed");
    float pitch=0,up=0;
    const float q[]={std::sin(15*pi/180),0,0,std::cos(15*pi/180)};
    Check(BikePitchFromQuaternion(q,pitch,up),"valid attitude rejected");Near(pitch,30,"wrong game pitch convention");
    const float bad[]={0,0,0,0};Check(!BikePitchFromQuaternion(bad,pitch,up),"zero quaternion used");
}
void BikeWheelieLimit() {
    BikeWheelieAssist limit;auto a=Ground();a.pitch=12;
    a.rate=250;auto out=limit.Apply(-1,.8f,a,a.stampUs,10);Near(out.lean,-1,"ground lean suppressed");Near(out.throttle,.8f,"hill cut throttle");
    a.rate=0;a.frontContact=false;a.contacts=1;a.pitch=32;a.stampUs+=11111;
    out=limit.Apply(-1,.8f,a,a.stampUs,35);Near(out.relativePitch,20,"hill not subtracted");Near(out.throttle,.8f,"early throttle cut");
    a.pitch=47;a.stampUs+=11111;
    out=limit.Apply(-1,.8f,a,a.stampUs,35);Near(out.lean,0,"rear lean retained at limit");Near(out.throttle,0,"throttle retained at limit");
    a.pitch=52;a.stampUs+=11111;out=limit.Apply(-1,.8f,a,a.stampUs,35);Check(out.lean>0,"no forward correction");
    a.rearContact=false;a.contacts=0;out=limit.Apply(-1,.8f,a,a.stampUs,35);Near(out.lean,-1,"airborne controls replaced");Near(out.throttle,.8f,"airborne throttle replaced");
    limit.Reset();a=Ground();limit.Apply(-1,1,a,a.stampUs,35);a.frontContact=false;a.pitch=20;a.rate=100;a.stampUs+=11111;
    out=limit.Apply(-1,1,a,a.stampUs,35);Check(out.predictedPitch>30 && out.throttle<1,"rising momentum ignored");
    out=limit.Apply(-1,.8f,a,a.stampUs+100001,35);Near(out.throttle,.8f,"stale attitude cuts throttle");
    BikeWheelieAssist unknown;a.frontContact=false;out=unknown.Apply(-1,.8f,a,a.stampUs,35);Near(out.throttle,.8f,"missing ground reference guessed");
}
void BikeHandsBalance() {
    BikeHandBalance balance;auto a=Ground();
    Near(balance.Apply(-.6f,-.4f,true,a,a.stampUs,.12f),-.6f,"ground hand motion overrode body");
    a.frontContact=false;a.pitch=15;a.stampUs+=11111;
    Near(balance.Apply(0,-.4f,true,a,a.stampUs,.12f),-.6f,"liftoff lost pitch initiation");
    Check(balance.Active(),"hands not engaged after liftoff");
    a.stampUs+=180000;
    Near(balance.Apply(-1,-.4f,true,a,a.stampUs,.12f),0,"launch lean stayed stuck with neutral hands");
    Near(balance.Apply(0,-.28f,true,a,a.stampUs,.12f),-1,"raising hands did not lift");
    Near(balance.Apply(0,-.52f,true,a,a.stampUs,.12f),1,"lowering hands did not lower");
    Check(balance.Apply(.8f,-.28f,true,a,a.stampUs,.12f)>=.8f,"hands blocked forward body override");
    a.frontContact=true;a.stampUs+=10000;balance.Apply(0,-.4f,true,a,a.stampUs,.12f);
    Check(balance.Active(),"one contact sample erased hand balance");
    a.frontContact=false;a.stampUs+=10000;
    Near(balance.Apply(-1,-.4f,true,a,a.stampUs,.12f),0,"contact bounce restarted full launch lean");
    Near(balance.Apply(0,-.28f,false,a,a.stampUs,.12f),0,"lost hand held pitch");
    Near(balance.Apply(0,-.28f,true,a,a.stampUs,.12f),0,"hand regrab in air guessed neutral");
    a.frontContact=true;balance.Apply(-.5f,-.4f,true,a,a.stampUs,.12f);a.frontContact=false;
    balance.Apply(-.5f,-.4f,true,a,a.stampUs,.12f);
    Near(balance.Apply(.1f,-.4f,true,a,a.stampUs,.12f,2),.1f,"recenter retained old hand phase");
    a.frontContact=true;balance.Apply(-.5f,-.4f,true,a,a.stampUs,.12f,2);a.frontContact=false;
    Near(balance.Apply(0,-.4f,true,a,a.stampUs+100001,.12f,2),0,"stale hands applied");
    // Hand height is torso-relative: translating the whole rider must not change it.
    TrackedBikeStunts tracked;auto f=Tracking(1,0,0);tracked.Update(3,f,f.stampUs,42);
    const float height=tracked.HandHeight();Check(tracked.HandsValid(f.stampUs),"valid hands rejected");
    f=Tracking(2,0,0);f.head.position.y+=.2f;tracked.Update(3,f,f.stampUs,42);Near(tracked.HandHeight(),height,"body rise counted as hand rise");
    f=Tracking(3,0,0);const auto rise=RotateVector(ConjugateQuat(f.head.orientation),XrVector3f{0,.1f,0});for(auto& h:f.hands){h.x+=rise.x;h.y+=rise.y;h.z+=rise.z;}tracked.Update(3,f,f.stampUs,42);Near(tracked.HandHeight(),height+.1f,"coordinated hand rise lost");
}
void BikeWheelieHoldPolicy() {
    BikeWheelieHold hold;auto a=Ground();a.pitch=12;
    auto apply=[&](float body=0,float height=-.4f,float throttle=.8f,float brake=0,bool hands=true,uint64_t origin=1){return hold.Apply(body,height,hands,throttle,brake,a,a.stampUs,.12f,origin,25,35);};
    auto out=apply(-.7f);Near(out.lean,-.7f,"hold altered grounded launch");Check(!out.active,"hold active on road");
    a.frontContact=false;a.pitch=24;a.stampUs+=10000;out=apply();Check(!out.active,"single lost contact armed hold");
    a.stampUs+=30000;out=apply();Check(out.active,"confirmed deliberate wheelie did not engage");Near(out.relativePitch,12,"hold ignored road slope");
    for(int i=0;i<60;++i){a.stampUs+=11111;out=apply();}
    Near(out.target,25,"neutral hands did not reach chosen hold target");Check(out.lean<0,"falling/below-target wheelie not lifted");
    const auto before=out;for(int i=0;i<20;++i)out=apply();Near(out.lean,before.lean,"repeated input polls advanced feedback");Near(out.target,before.target,"repeated polls moved target");
    a.pitch=47;a.rate=25;for(int i=0;i<30;++i){a.stampUs+=11111;out=apply();}Check(out.lean>0,"rising above target not corrected forward");
    for(int i=0;i<90;++i){a.stampUs+=11111;out=apply(0,-.28f);}Near(out.target,27,"raised hands bypassed angle margin");
    for(int i=0;i<90;++i){a.stampUs+=11111;out=apply(0,-.52f);}Near(out.target,13,"lowered hands did not lower target");
    a.frontContact=true;a.stampUs+=10000;Check(apply().active,"brief contact reset hold");a.frontContact=false;a.stampUs+=10000;Check(apply().active,"contact flicker lost hold");
    out=apply(0,-.4f,.8f,1);Check(!out.active,"braking did not immediately cancel hold");
    Check(!apply().active,"hold rearmed in air after braking");
    auto arm=[&](){hold.Reset();a=Ground();apply(-.7f);a.frontContact=false;a.pitch=15;a.stampUs+=10000;apply();a.stampUs+=30000;Check(apply().active,"test wheelie failed to arm");};
    arm();Check(!apply(.3f).active,"forward lean did not cancel");Check(!apply().active,"forward cancel rearmed midair");
    arm();Check(!apply(0,-.4f,0).active,"throttle release did not cancel");
    arm();Check(!apply(0,-.4f,.8f,0,false).active,"hand tracking loss held pitch");
    arm();a.rearContact=false;Check(!apply().active,"fully airborne bike held pitch");
    arm();a.vehicle=43;Check(!apply().active,"vehicle switch retained hold");
    arm();Check(!apply(0,-.4f,.8f,0,true,2).active,"recenter retained hold");
    arm();out=hold.Apply(0,-.4f,true,.8f,0,a,a.stampUs+100001,.12f,1,25,35);Check(!out.active,"stale native sample held pitch");
    arm();a.stampUs+=10000;a.frontContact=true;apply();a.stampUs+=30000;Check(!apply().active,"confirmed landing did not stop hold");
    arm();a.valid=false;Check(!apply().active,"invalid attitude held pitch");
    arm();out=apply(0,std::numeric_limits<float>::quiet_NaN());Check(!out.active&&std::isfinite(out.lean),"invalid hand sample polluted native input");
    BikeWheelieHold unknown;a=Ground();a.frontContact=false;a.pitch=25;out=unknown.Apply(-1,-.4f,true,1,0,a,a.stampUs,.12f,1,25,35);Check(!out.active,"enabled in air guessed road reference");
}
void BikeWheelieHoldResponse() {
    float results[4]{};int index=0;
    // A deliberately simple pitch plant checks feedback direction, damping and
    // sample-rate consistency. It is NOT a claim about Cyberpunk's vehicle tuning.
    for(int hz:{30,60,90,144}) {
        BikeWheelieHold hold;auto a=Ground();const uint64_t step=1000000/hz;
        hold.Apply(-1,-.4f,true,.8f,0,a,a.stampUs,.12f,1,25,35);
        a.frontContact=false;a.pitch=12;a.rate=20;
        float peak=a.pitch;
        for(int i=0;i<hz*6;++i) {
            a.stampUs+=step;
            const auto out=hold.Apply(0,-.4f,true,.8f,0,a,a.stampUs,.12f,1,25,35);
            if(i>hz/5)Check(out.active,"hold lost valid sustained wheelie");
            Check(std::isfinite(out.lean)&&std::abs(out.lean)<=1,"hold output unbounded");
            if(i==hz*3)a.rate-=15; // bump/torque disturbance, not controller input
            const float dt=1.f/hz;
            a.rate+=(-80.f*out.lean-.8f*a.rate-10.f)*dt;
            a.pitch+=a.rate*dt;peak=std::max(peak,a.pitch);
        }
        Check(std::abs(a.pitch-25)<5,"nominal model failed to settle near target");
        Check(peak<40,"nominal model has excessive overshoot");results[index++]=a.pitch;
    }
    for(float value:results)Check(std::abs(value-results[0])<2,"hold depends excessively on polling rate");
}
void BikeSeatMirror() {
    for(int sign:{-1,1}) {
        cvr::camera::BikeSeat seat;
        int32_t owner[]={1000000000,-1000000000,1000000};
        int32_t camera[]={owner[0],owner[1],owner[2]+131072};
        const float q0[]={0,0,0,1};
        Check(!seat.Apply(camera,owner,q0,42,7,9,true,true),"baseline moved camera");
        const float angle=sign*25*pi/180;
        // Bank about local +Y. Fresh native camera has an extra +/-20cm sway.
        const float q[]={0,std::sin(angle/2),0,std::cos(angle/2)};
        owner[0]+=123456;owner[1]-=54321;
        const double r[]={std::cos(angle),0,-std::sin(angle)},u[]={std::sin(angle),0,std::cos(angle)};
        for(int i=0;i<3;++i)camera[i]=owner[i]+int32_t(std::llround((u[i]+sign*.2*r[i])*131072));
        Check(seat.Apply(camera,owner,q,42,7,9,true,false),"seat not stabilized");
        for(int i=0;i<3;++i)Check(std::abs(int64_t(camera[i])-owner[i]-std::llround(u[i]*131072))<3,"seat mirrored bank mismatch");
        Near(seat.CorrectionMeters(),-sign*.2f,"seat correction not symmetric");
        const int32_t before=camera[0];seat.Apply(camera,owner,q,42,7,9,false,false);Check(before==camera[0],"disabled seat changed position");
        Check(!seat.Apply(camera,owner,q,42,7,9,true,false),"seat captured while banked");
    }
}

}
int main(int argc,char** argv) {
    if(argc!=2)return 2;const std::string name=argv[1];
    if(name=="bike_initial_pair_neutral")BikeInitialPairNeutral();else if(name=="bike_look_aside_neutral")BikeLookAsideNeutral();else if(name=="bike_lateral_contact")BikeLateralContact();else if(name=="bike_lateral_coupling")BikeLateralCoupling();else if(name=="bike_lean_ramp")BikeLeanRamp();else if(name=="bike_pair_recovery")BikePairRecovery();else if(name=="bike_pair_tracking")BikePairTracking();else if(name=="bike_wheelie_hold")BikeWheelieHoldPolicy();else if(name=="bike_wheelie_hold_response")BikeWheelieHoldResponse();else if(name=="bike_contact_steering")BikeContactSteering();else if(name=="bike_wheelie_limit")BikeWheelieLimit();else if(name=="bike_hands_balance")BikeHandsBalance();else if(name=="bike_seat_mirror")BikeSeatMirror();else if(name=="grab_offset")GrabOffset();else if(name=="one_hand")OneHand();
    else if(name=="two_hands")TwoHands();else if(name=="handoff")Handoff();
    else if(name=="lock_reversal")LockReversal();else if(name=="tracking_loss")TrackingLoss();
    else if(name=="rate_matrix")RateMatrix();else if(name=="deadzone")Deadzone();
    else if(name=="extended_travel")ExtendedTravel();
    else if(name=="gamepad_range")GamepadRange();else if(name=="tracked_head_motion")TrackedHeadMotion();
    else if(name=="tracked_roundtrip")TrackedRoundtrip();else if(name=="handlebar")Handlebar();else if(name=="bike_lean_lifecycle")BikeLeanLifecycle();else if(name=="bike_head_tilt")BikeHeadTilt();else if(name=="tracked_lifetime")TrackedLifetime();
    else if(name=="bike_direct_bank")BikeDirectBank();else if(name=="bike_bank_policy")BikeBankPolicy();
    else if(name=="bike_stunt_motion")BikeStuntMotion();else if(name=="bike_stunt_lifecycle")BikeStuntLifecycle();else if(name=="bike_stunt_native")BikeStuntNativeInput();
    else if(name=="tracked_freeze")TrackedFreeze();
    else if(name=="prediction_constant")PredictionConstant();else if(name=="prediction_stop_reverse")PredictionStopReverse();
    else if(name=="prediction_boundaries")PredictionBoundaries();else if(name=="prediction_freshness")PredictionFreshness();
    else if(name=="prediction_tracking")PredictionTracking();else if(name=="prediction_jitter")PredictionJitter();
    else if(name=="prediction_model")PredictionModel();else return 2;
    std::cout<<"PASS "<<name<<'\n';
}
