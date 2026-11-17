#include "Runtimes/Haptics.hpp"
// CP2077 2.5: additive bank target, before the engine's PID/tilt-speed controller.
// REA evidence and full source-byte provenance: docs/BIKE-DIRECT-BANK.md.
#include "Hooks/BikeBank.hpp"
#include "Hooks/BikeBankSites.hpp"
#include "Anim/BikeBankPolicy.hpp"
#include "Anim/WheelGrab.hpp"
#include "Camera/CameraState.hpp"
#include "Camera/PlayerCameraState.hpp"
#include "Core/GameBuild.hpp"
#include "Runtimes/OpenXRManager.hpp"
#include "Core/LiveControls.hpp"
#include "Core/VrCoreShared.hpp"
#include "Hooks/Hook.hpp"
#include "Overlay/ImGuiOverlay.hpp"
#include "Utils/MemorySafe.hpp"
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <mutex>

extern void Log(const char*,...);
namespace cvr::bikebank {
namespace {
using TargetFn=float(__fastcall*)(uintptr_t,float);
using ContactsFn=int(__fastcall*)(uintptr_t);
TargetFn original{};
ContactsFn contacts{};
std::atomic<bool> ready{false};
std::mutex sampleMutex;
Sample sample{};
std::atomic<bool> motionReady{false};
std::mutex motionMutex;
cvr::anim::BikeAttitude motion{};
void ObserveMotion(uintptr_t model,uintptr_t owner,float dt) {
    cvr::anim::BikeAttitude next{};next.stampUs=XrDiagNowUs();next.vehicle=owner;
    float custom=0;
    next.scriptedTilt=!ReadFloatSafe(model+0xE24,&custom)||!std::isfinite(custom)||custom!=0;
    uintptr_t physical{},state{};float q[4]{};
    if(motionReady.load(std::memory_order_relaxed) && ReadU64Safe(model+0x60,&physical) && physical &&
       ReadU64Safe(physical+0x2D8,&state) && state &&
       ReadFloatArraySafe(reinterpret_cast<const float*>(state+0x160),q,4) &&
       cvr::anim::BikePitchFromQuaternion(q,next.pitch,next.upZ)) {
        next.valid=ReadFloatSafe(model+0xE28,&next.roll) && std::isfinite(next.roll);
        next.roll/=kRadPerDegree;next.contacts=contacts(model);
    }
    // Contact is independent of chassis attitude: an unusable pose must not
    // restore road rumble or handlebar steering while the native wheel is up.
    uint32_t count{};float front{},rear{};
    if(motionReady.load(std::memory_order_relaxed) && ReadU32Safe(model+0xC20,&count) && count==2 &&
       ReadFloatSafe(model+0x5E0+0xB4,&front) && ReadFloatSafe(model+0x5E0+0x190+0xB4,&rear)) {
        next.frontGap=front;next.rearGap=rear;next.wheelsValid=true;
        next.frontContact=cvr::anim::BikeNativeWheelContact(front);
        next.rearContact=cvr::anim::BikeNativeWheelContact(rear);
        next.contacts=int(next.frontContact)+int(next.rearContact);
    }
    next.wheelCount=count;
    std::lock_guard lock(motionMutex);
    if(next.valid && motion.valid && motion.vehicle==owner && next.stampUs>=motion.stampUs &&
       next.stampUs-motion.stampUs<=100000 && std::isfinite(dt) && dt>=.001f && dt<=.1f) {
        const float rate=std::clamp((next.pitch-motion.pitch)/dt,-360.f,360.f);
        next.rate=motion.rate+(rate-motion.rate)*(dt/(dt+.04f));
    }
    motion=next;
}
void Record(Sample next) {
    std::lock_guard lock(sampleMutex);sample=next;
    // Slow periodic sample plus bounded grip/contact transitions, not a per-frame log.
    static uint64_t lastLog=0;static unsigned previous=~0u;
    const unsigned state=next.reason|(unsigned(next.held)<<4)|(unsigned(next.front)<<6)|
        (unsigned(next.rear)<<7)|(unsigned(next.contactValid)<<8);
    const auto elapsed=next.stampUs>=lastLog?next.stampUs-lastLog:0;
    if(lastLog && elapsed<5000000 && (state==previous || elapsed<250000))return;
    previous=state;lastLog=next.stampUs;
    Log("[bike-bank] native=%.2f add=%.2f target=%.2f actual=%.2f limit=%.2f steer=%.3f reason=%u bars=%.3f lean_turn=%.3f held=%u contacts=%d/%d valid=%d\n",
        next.nativeDegrees,next.requestedDegrees,next.targetDegrees,next.actualDegrees,next.maxDegrees,next.steering,next.reason,next.barShare,next.leanSteer,unsigned(next.held),int(next.front),int(next.rear),int(next.contactValid));
}
float __fastcall Target(uintptr_t model,float dt) {
    // Original owns its internal smoothing state and all native/scripted cases.
    const float native=original(model,dt);
    if(!(g_liveControls.xrBikeDirectBank || g_liveControls.xrBikeStunts || g_liveControls.xrBikeAirLeanOnly || g_liveControls.xrBikeStableSeat || cvr::haptics::VehicleGain()>0) || !g_isDriving.load(std::memory_order_relaxed) ||
       !g_isOnBike.load(std::memory_order_relaxed) || g_menuModeValue!=0 ||
       g_sceneTier.load(std::memory_order_relaxed)>2 || OverlayIsVisible())return native;
    uintptr_t owner=0;
    // +DE0 is the BikeObject cached by the game's dynamic cast at 6EE54C..556.
    // Compare to the mounted identity; never dereference a remembered vehicle.
    if(!ReadU64Safe(model+0xDE0,&owner) || !owner || owner!=CurrentMountedVehicleIdentity())return native;
    uintptr_t confirmedModel{};
    if(!ReadU64Safe(owner+0x2D0,&confirmedModel)||confirmedModel!=model)return native;
    ObserveMotion(model,owner,dt);
    if(!g_liveControls.xrBikeDirectBank)return native;
    Sample out{};out.stampUs=XrDiagNowUs();out.nativeDegrees=native/kRadPerDegree;
    out.targetDegrees=out.nativeDegrees;out.reason=1;
    const auto contact=LastMotion();out.contactValid=contact.wheelsValid;
    out.front=contact.frontContact;out.rear=contact.rearContact;
    const auto input=cvr::anim::WheelLastBikeSample();
    out.barShare=input.barShare;out.leanSteer=input.turnShare;out.held=input.held;
    float custom=0,actual=0;uintptr_t back=0;
    if(!ReadU64Safe(owner+0x2D0,&back) || back!=model ||
       !ReadFloatSafe(model+0xE24,&custom) || !ReadFloatSafe(model+0xE64,&out.maxDegrees) ||
       !ReadFloatSafe(model+0xE28,&actual) || !ReadFloatSafe(owner+0x278,&out.steering) ||
       !std::isfinite(native) || !std::isfinite(custom)) {Record(out);return native;}
    out.actualDegrees=actual/kRadPerDegree;
    // Scripted custom target replaces native bank, so don't fight a quest or mod.
    if(custom!=0) {out.reason=2;Record(out);return native;}
    // The game returns zero natural bank without wheel contact; do not introduce
    // our own balance torque in midair or replace the native air controls.
    if(contacts(model)<=0) {out.reason=3;Record(out);return native;}
    if(!(cvr::anim::WheelControlState()&3) || !input.bankValid || !input.directBank ||
       input.headTiltMode!=(g_liveControls.xrBikeHeadTilt!=0) || input.vehicleIdentity!=owner ||
       !Fresh(out.stampUs,input.stampUs)) {out.reason=4;Record(out);return native;}
    if(!std::isfinite(out.maxDegrees) || out.maxDegrees<1 || out.maxDegrees>89) {Record(out);return native;}
    out.requestedDegrees=input.bankOffsetDegrees;
    const float result=CoordinatedTarget(native,out.requestedDegrees,out.maxDegrees);
    out.targetDegrees=result/kRadPerDegree;out.reason=0;Record(out);
    return result;
}
bool Install() {
    if(!game_build::Supported() || game_build::Overridden())return false;
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    for(const auto& site:sites::kGuards) {
        if(std::memcmp(reinterpret_cast<const void*>(base+site.rva),site.bytes,site.size)!=0) {
            Log("[bike-bank] byte guard failed at RVA %llx; direct bank unavailable\n",static_cast<unsigned long long>(site.rva));
            return false;
        }
    }
    motionReady.store(std::memcmp(reinterpret_cast<const void*>(base+sites::kPoseSite),sites::kPoseBytes,sizeof(sites::kPoseBytes))==0 &&
        std::memcmp(reinterpret_cast<const void*>(base+sites::kWheelRangeSite),sites::kWheelRangeBytes,sizeof(sites::kWheelRangeBytes))==0 &&
        std::memcmp(reinterpret_cast<const void*>(base+sites::kFrontWheelSite),sites::kFrontWheelBytes,sizeof(sites::kFrontWheelBytes))==0 &&
        std::memcmp(reinterpret_cast<const void*>(base+sites::kGroundContactSite),sites::kGroundContactBytes,sizeof(sites::kGroundContactBytes))==0,std::memory_order_release);
    Log("[bike-bank] chassis pitch measurement guard=%d\n",motionReady.load()?1:0);
    contacts=reinterpret_cast<ContactsFn>(base+sites::kContacts);
    auto* target=reinterpret_cast<void*>(base+sites::kTarget);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&Target),reinterpret_cast<void**>(&original))!=MH_OK)return false;
    if(MH_EnableHook(target)!=MH_OK) {MH_RemoveHook(target);original=nullptr;return false;}
    ready.store(true,std::memory_order_release);
    Log("[bike-bank] direct target hook ready; coordinated physical lean + native steering bank, native balance controller retained\n");
    return true;
}
CVR_HOOK("BikeBank",::cvr::hooks::Stage::Boot,83,Install);
}
bool MotionReady() {return ready.load(std::memory_order_acquire)&&motionReady.load(std::memory_order_acquire);}
cvr::anim::BikeAttitude LastMotion() {std::lock_guard lock(motionMutex);return motion;}
bool Ready() {return ready.load(std::memory_order_acquire);}
Sample LastSample() {std::lock_guard lock(sampleMutex);return sample;}
}
