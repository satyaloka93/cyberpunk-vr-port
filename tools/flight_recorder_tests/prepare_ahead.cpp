#include "Framegen/Presenter.hpp"
#include "Framegen/FlightRecorder.hpp"
#include "Framegen/RefreshCadence.hpp"
#include <iostream>
#include <cstdlib>
using namespace cvr::framegen;
extern "C" int CyberpunkVR_RuntimeDiagnostics=0;
void Log(const char*,...){}
static Settings settings;static bool targeted=true,compatible=true,outputReady=true,joinOk=true;
static bool steamvr=true,resetReady=true;static unsigned consumed{},submitted{},rejected{};
static unsigned dispatched{},joined{},deferred{};static double clockMs=100;
namespace cvr::framegen::refresh {bool Targeted(){return targeted;}bool SteamVrSession(){return steamvr;}void SetEnabled(bool on){targeted=steamvr&&on;}}
namespace cvr::framegen::flight {void Count(Event e){if(e==Event::SubmitRejected)++rejected;}void Time(Timing,double){}void Output(unsigned,unsigned){}}
namespace cvr::framegen {
struct Generator::Impl{};
Generator::Generator()=default;Generator::~Generator()=default;
bool Generator::Generate(ID3D12Device*,ID3D12CommandQueue*,ID3D12Resource*[2],const std::shared_ptr<const Inputs>(&)[2],const EyePair(&)[2],const Settings&,double,bool reset,uint64_t,bool* generated,ID3D12Fence*,uint64_t,std::shared_ptr<void>,ID3D12CommandQueue*,std::shared_ptr<const CaptureTiming>,bool defer){
 ++dispatched;if(defer)++deferred;else ++joined;if(generated)*generated=!reset;return true;
}
bool Generator::CanPrepare(ID3D12Device*,ID3D12CommandQueue*,ID3D12Resource* const[2],const std::shared_ptr<const Inputs>(&)[2],const Settings&)const{return compatible;}
bool Generator::JoinOutput(ID3D12CommandQueue*){++joined;return joinOk;}
bool Generator::OutputReady()const{return outputReady;}
ID3D12Resource* Generator::Output(unsigned i)const{return reinterpret_cast<ID3D12Resource*>(uintptr_t(100+i));}
void Generator::ConsumerFence(ID3D12Fence*,uint64_t){++consumed;}void Generator::Poll(){}bool Generator::Reset(){return resetReady;}void Generator::Shutdown(){}
Settings GetSettings(){return settings;}Backend ActiveBackend(){return settings.backend;}
bool MetricsEnabled(){return true;}bool DepthCaptureEnabled(){return false;}void ReleaseInputs(){}uint64_t InputVram(){return 0;}void SetVram(uint64_t){}
void PacingReady(bool){}void ResetPacingHistory(){}void OnPresentationCycle(bool){}void Status(const char*,bool){}void OnSkip(){}
void OnSubmitted(uint64_t,bool,bool,double){++submitted;}double NowMs(){return clockMs;}
void CaptureFrame(ID3D12Device*,ID3D12CommandQueue*,ID3D12Resource* const[2],const std::shared_ptr<const Inputs>(&)[2],const EyePair(&)[2]){}
}
void check(bool v,const char* why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
Frame frame(uint64_t n){
 Frame f;f.identity.serial=n;f.identity.origin=1;f.identity.nativeFrame=uint32_t(n);f.identity.capturedMs=n*22.22;
 f.identity.pose[0]=n*2;f.identity.pose[1]=n*2+1;f.identity.stereo=f.identity.inputs=true;
 f.lease=std::make_shared<ReadLease>();
 for(unsigned i=0;i<2;++i){f.color[i]=reinterpret_cast<ID3D12Resource*>(uintptr_t(1+i));f.pose[i].orientation.w=1;
  f.fov[i]={-.7f,.7f,.7f,-.7f};f.inputs[i]=std::make_shared<Inputs>();}
 return f;
}
int main(){
 settings.enabled=settings.autoPace=true;settings.backend=Backend::Nvidia;
 auto* device=reinterpret_cast<ID3D12Device*>(1);auto* queue=reinterpret_cast<ID3D12CommandQueue*>(2);auto* consumer=reinterpret_cast<ID3D12CommandQueue*>(3);
 Presenter p;auto a=frame(1);check(!p.Prepare(device,queue,a,consumer),"cannot prepare without primed history");
 auto real=p.Select(device,queue,a,true,consumer);check(!real.synthetic,"first real primes");p.Submitted(real,nullptr,0,true);
 auto b=frame(2);auto bLease=std::weak_ptr<ReadLease>(b.lease);const auto before=joined;
 check(p.Prepare(device,queue,b,consumer),"next capture prepared in idle time");b={};
 check(deferred==1 && joined==before,"preparation must not block current real presentation queue");
 check(!p.Prepare(device,queue,frame(3),consumer),"single prepared pair cannot be overwritten");
 clockMs+=9;auto mid=p.Select(device,queue,frame(3),true,consumer);
 check(mid.synthetic && mid.frame.identity.serial==2 && joined==before+1,"prepared midpoint takes priority over newer capture and joins only now");
 p.Submitted(mid,nullptr,0,true);mid={};check(!bLease.expired(),"real endpoint pinned after midpoint");
 real=p.Select(device,queue,frame(3),true,consumer);check(!real.synthetic && real.frame.identity.serial==2,"matching B endpoint follows midpoint");
 // Prepare C while B is being displayed. Pending-real acceptance must not clear C.
 check(p.Prepare(device,queue,frame(3),consumer),"prepare following pair during real endpoint");
 p.Submitted(real,nullptr,0,true);real={};check(bLease.expired(),"B lease released after matching endpoint");
 mid=p.Select(device,queue,frame(4),true,consumer);check(mid.synthetic && mid.frame.identity.serial==3,"C preparation survives B acceptance");
 p.Submitted(mid,nullptr,0,true);real=p.Select(device,queue,frame(4),true,consumer);p.Submitted(real,nullptr,0,true);
 targeted=false;check(!p.Prepare(device,queue,frame(4),consumer),"VDXR excluded");targeted=true;
 settings.autoPace=false;check(!p.Prepare(device,queue,frame(4),consumer),"disabled adaptive excluded");settings.autoPace=true;
 compatible=false;check(!p.Prepare(device,queue,frame(4),consumer),"reconfiguration cannot occur inside idle hold");compatible=true;
 auto reset=frame(4);reset.identity.reset=true;check(!p.Prepare(device,queue,reset,consumer),"reset capture not prefetched");
 check(!p.Prepare(device,queue,frame(5),consumer),"skipped neighbor not prefetched");
 check(p.Prepare(device,queue,frame(4),consumer),"prepare before cancellation");
 const auto n=joined;settings.enabled=false;real=p.Select(device,queue,frame(4),true,consumer);
 check(!real.synthetic && joined==n,"disabled FG cancels prepared image without output join");
 settings.enabled=true;real=p.Select(device,queue,frame(10),true,consumer);p.Submitted(real,nullptr,0,true);
 check(p.Prepare(device,queue,frame(11),consumer),"new history after toggle");
 auto changed=frame(11);changed.identity.origin=2;real=p.Select(device,queue,changed,true,consumer);
 check(!real.synthetic,"recenter cancels prepared pair");
 // Settings must wait for generation AND consumption to finish, preserving leases.
 p.Reset();settings.enabled=true;real=p.Select(device,queue,frame(20),true,consumer);p.Submitted(real,nullptr,0,true);real={};
 auto next=frame(21);auto pinned=std::weak_ptr<ReadLease>(next.lease);
 check(p.Prepare(device,queue,next,consumer),"prepared image before toggle");next={};
 settings.enabled=false;++settings.restartSerial;settings.steamVrOptimizations=false;resetReady=false;
 check(!p.ApplySteamVrSettings() && targeted,"in-flight work defers routing change");
 check(!pinned.expired(),"in-flight prepared endpoint remains pinned");
 const auto gpuWork=dispatched,oldConsumer=consumed;
 real=p.Select(device,queue,frame(22),true,consumer);check(!real.synthetic,"drain shows real frame");
 p.Submitted(real,nullptr,0,true);const auto delivered=submitted;
 p.Submitted(real,nullptr,0,false);check(submitted==delivered && rejected>0,"rejected drain submission never counted as delivered");
 real={};check(consumed==oldConsumer,"drain must not advance old consumer fence");
 settings.enabled=true; // quick off/on before GPU becomes idle must still be honored
 check(!p.ApplySteamVrSettings() && targeted,"fast restart still waits for old GPU work");
 check(!p.Prepare(device,queue,frame(23),consumer) && dispatched==gpuWork,"no preparation while draining");
 resetReady=true;check(p.ApplySteamVrSettings() && !targeted,"idle boundary changes timing/synthesis");
 check(pinned.expired(),"endpoint released only after safe reset");
 real=p.Select(device,queue,frame(30),true,consumer);check(!real.synthetic,"new route starts with real history");p.Submitted(real,nullptr,0,true);
 settings.steamVrOptimizations=true;check(!p.ApplySteamVrSettings() && !targeted,"edit while FG on remains pending");
 settings.enabled=false;++settings.restartSerial;check(p.ApplySteamVrSettings() && targeted,"FG-off applies pending enable");
 settings.enabled=true;check(!p.ApplySteamVrSettings(),"stable settings do not reset every frame");
 // Toggle between display cycles without Select ever observing disabled FG.
 ++settings.restartSerial;settings.steamVrOptimizations=false;
 check(p.ApplySteamVrSettings() && !targeted,"off/on edge remembered between XR cycles");
 steamvr=false;settings.enabled=false;settings.steamVrOptimizations=true;
 check(!p.ApplySteamVrSettings() && !targeted,"VDXR never activates SteamVR path");
 p.Shutdown();std::cout<<"PASS: production Presenter prepares without consumer join, ordered midpoint/endpoint, lease lifetime, toggle/recenter and backend/config gating\n";
}
