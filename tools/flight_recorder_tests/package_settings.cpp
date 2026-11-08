#include "Framegen/Framegen.hpp"
#include "Framegen/RefreshCadence.hpp"
#include "Framegen/FlightRecorder.hpp"
#include "xrfg/steamvr_delivery.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>
#include <iostream>
using namespace cvr::framegen;
extern "C" {int CyberpunkVR_RuntimeDiagnostics=0;}
namespace cvr::framegen {
void WakeHardwareTelemetry(){} void StopHardwareTelemetry(){}
HardwareStatistics GetHardwareStatistics(){return {};}
namespace flight {
void Count(Event){} void Configure(bool){} void Time(Timing,double){}
void Input(unsigned,uint32_t,uint32_t){} void Period(double){} void Pace(double,bool){}
}
}
namespace cvr::afw {bool Launch(){return false;}}
namespace xrfg {
struct SteamVrDelivery::Impl {};
SteamVrDelivery::SteamVrDelivery(bool) noexcept{}
SteamVrDelivery::~SteamVrDelivery(){}
void SteamVrDelivery::mark_established() noexcept{}
std::optional<std::chrono::nanoseconds> SteamVrDelivery::display_period() noexcept {return std::chrono::nanoseconds(11111111);}
}
void check(bool v,const char* why){if(!v){std::cerr<<why<<'\n';refresh::Stop();std::exit(1);}}
Settings roundtrip(Settings before) {
 FILE* file{};check(tmpfile_s(&file)==0 && file,"settings temp file");
 WriteSettings(file,before);rewind(file);Settings after;char line[512]{};
 while(fgets(line,sizeof(line),file))check(ParseSetting(line,after),"written key parses");fclose(file);return after;
}
int main(){
 Settings requested;check(requested.steamVrOptimizations && PresentationIsolation(requested),"existing default on");
 for(bool timing:{false,true})for(bool isolation:{false,true})for(bool adaptive:{false,true}) {
  requested.steamVrOptimizations=timing;requested.steamVrPresentationIsolation=isolation;requested.autoPace=adaptive;
  const auto loaded=roundtrip(requested);
  check(loaded.steamVrOptimizations==timing && PresentationIsolation(loaded)==isolation && loaded.autoPace==adaptive,"three independent settings roundtrip");
 }
 Settings old;ParseSetting("xr_framegen_steamvr_optimizations=0",old);SetSettings(old);
 check(!PresentationIsolation(GetSettings()),"legacy combined OFF preserves presentation opt-out");
 old.steamVrOptimizations=true;SetSettings(old);check(PresentationIsolation(GetSettings()),"legacy combined ON migrates");
 Settings explicitOff;ParseSetting("xr_framegen_steamvr_presentation_isolation=0",explicitOff);
 ParseSetting("xr_framegen_steamvr_optimizations=1",explicitOff);SetSettings(explicitOff);
 check(!PresentationIsolation(GetSettings()),"explicit isolation takes precedence regardless of order");
 check(!ParseSetting("xr_framegen_steamvr_optimizations=oops",requested),"invalid value ignored");
 check(!ParseSetting("xr_framegen_steamvr_presentation_isolation=oops",requested),"invalid isolation ignored");
 requested=GetSettings();requested.enabled=true;SetSettings(requested);const auto before=GetSettings().restartSerial;
 requested.enabled=false;SetSettings(requested);requested.enabled=true;SetSettings(requested);
 check(GetSettings().restartSerial==before+2,"quick off/on retained even with stale UI snapshot");
 requested=GetSettings();requested.enabled=false;SetSettings(requested);const auto stopped=GetSettings().restartSerial;
 requested=GetSettings();requested.steamVrOptimizations=!requested.steamVrOptimizations;requested.enabled=true;SetSettings(requested);
 check(GetSettings().restartSerial==stopped+1,"enable from already-OFF captures a pending edit before the next XR cycle");
 const auto finalEpoch=GetSettings().restartSerial;
 requested=GetSettings();requested.restartSerial=99999;SetSettings(requested);
 check(GetSettings().restartSerial==finalEpoch,"UI cannot overwrite internal restart epoch");
 auto persisted=roundtrip(GetSettings());check(persisted.restartSerial==0,"restart epoch not persisted");
 refresh::Start(true,false);requested.steamVrOptimizations=true;SetSettings(requested);
 check(!refresh::Targeted(),"UI edit alone never changes active timing");
 refresh::SetEnabled(true);check(refresh::Targeted() && refresh::SteamVrSession(),"drained boundary enables timing without session restart");
 refresh::SetEnabled(false);check(!refresh::Targeted() && refresh::Resolve(22222222)==22222222,"drained boundary disables timing");
 refresh::SetEnabled(true);check(refresh::Targeted(),"timing can re-enable in same session");
 refresh::Stop();check(!refresh::SteamVrSession(),"stop clears runtime identity");
 refresh::Start(false);for(bool on:{false,true}){refresh::SetEnabled(on);check(!refresh::Targeted(),"VDXR bypasses live timing changes");}
 refresh::Stop();std::cout<<"PASS: independent settings, legacy migration, restart epoch and dynamic runtime-gated timing\n";
}
