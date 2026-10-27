#include "Runtimes/Haptics.hpp"
#include "Runtimes/HapticRouting.hpp"
#include "Runtimes/OpenXRManager.hpp"
#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
extern "C" {
int CyberpunkVR_TwoHandActive = 0;
float CyberpunkVR_WeaponKickDeg = 1;
extern int CyberpunkVR_Haptics;
extern float CyberpunkVR_HapticScale;
}
std::atomic<bool> g_isDriving{false};
struct Output { int hand; float amp; float seconds; };
std::vector<Output> output;
int OpenXRManager::ApplyHaptic(int hand, float amp, float sec) { output.push_back({hand,amp,sec}); return 0; }
int checks = 0;
void check(bool condition, const char* why) { ++checks; if (!condition) { std::cerr << why << '\n'; std::exit(1); } }
int main() {
    using namespace cvr::haptics;
    HapticRoute().BeginSession(false, false);
    SetLegacyGain(1.25f); SetVehicleGain(1);
    Pulse(1,.45f,45); Pulse(1,1,90); Tick();
    check(output.size()==1 && output[0].hand==1 && output[0].amp==1 && output[0].seconds>.08f,"Quest swing merged and dispatched");
    g_isDriving=true; SetVehicleHaptics(.1f,.1f); output.clear(); Tick();
    check(output.size()==1 && output[0].hand==0,"vehicle does not overwrite active right swing");
    Clear(); output.clear(); Shot(2,true); Tick();
    check(output.size()==2,"two-hand actual shot uses both hands");
    check(std::abs(output[0].amp/output[1].amp-.55f)<.001f,"old weaker support-hand ratio restored");
    Clear(); output.clear(); Play(kMeleeParry,kRight); Pulse(1,.5f,45); Tick();
    check(output.size()==1 && output[0].amp==1 && output[0].seconds<.021f,"upstream parry outranks swing");
    Clear(); output.clear(); SetVehicleHaptics(.3f,.4f); Tick();
    check(output.size()==2 && output[0].seconds==.045f,"vehicle supplies both hands");
    output.clear(); g_isDriving=false; Tick(); check(output.empty(),"vehicle stops on exit even with fresh audio");
    g_isDriving=true; Sleep(110); Tick(); check(output.empty(),"stale vehicle expires");
    Pulse(1,1,90); Sleep(110); Tick(); check(output.empty(),"stale swing expires");
    Clear(); Pulse(1,.8f,90); CyberpunkVR_Haptics=0; Tick();
    check(output.empty(),"master mute clears queued swing");
    CyberpunkVR_Haptics=1; Tick(); check(output.empty(),"unmute never replays old swing");
    SetVehicleHaptics(.3f,.4f); SetVehicleGain(0); Tick();
    check(output.empty(),"vehicle gain zero mutes existing audio");
    Clear(); HapticRoute().Request(2); HapticRoute().BeginSession(true,true);
    SetVehicleGain(1); SetVehicleHaptics(.5f,.5f); Shot(2,true); Tick();
    check(output.empty(),"Toolkit owns vehicle and shot effects exclusively");
    HapticRoute().BeginSession(true,false); Clear(); Pulse(0,.6f,85); Tick();
    check(output.size()==1 && output[0].hand==0,"PSVR2 without bridge receives generic swing");
    Clear(); output.clear(); Play(kBeltTouch,kLeft); Clear(); Tick();
    check(output.empty(),"focus/session clear drops all envelope events");
    std::cout << checks << " production haptic playback checks passed\n";
}
