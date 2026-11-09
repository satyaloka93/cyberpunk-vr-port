#ifdef CVR_V557_RECONSTRUCTION
#include "Framegen/V557Generator.hpp"
#include "Framegen/SynthesisQueue.hpp"
#include "Framegen/RefreshCadence.hpp"
#include "Framegen/FlightRecorder.hpp"
#include "Framegen/FrameCapture.hpp"
#include <xrfg/d3d12_frame_synthesizer.hpp>
#include <wrl/client.h>
#include <array>

extern void Log(const char*,...);
namespace cvr::framegen {
namespace {
using Microsoft::WRL::ComPtr;
bool Done(ID3D12Fence* fence,uint64_t value) {
    if(!value)return true;
    const auto completed=fence?fence->GetCompletedValue():UINT64_MAX;
    return completed!=UINT64_MAX && completed>=value;
}
DXGI_FORMAT ViewFormat(DXGI_FORMAT f) {
    if(f==DXGI_FORMAT_R8G8B8A8_TYPELESS)return DXGI_FORMAT_R8G8B8A8_UNORM;
    if(f==DXGI_FORMAT_B8G8R8A8_TYPELESS)return DXGI_FORMAT_B8G8R8A8_UNORM;
    return f;
}
xrfg::D3D12ReprojectionView View(const EyeCamera& c,const D3D12_RESOURCE_DESC& d) {
    xrfg::D3D12ReprojectionView v;
    v.pose.orientation={c.orientation[0],c.orientation[1],c.orientation[2],c.orientation[3]};
    v.fov={c.fov[0],c.fov[1],c.fov[2],c.fov[3]};
    v.image_rect={0,0,UINT(d.Width),d.Height};v.array_slice=0;return v;
}
}
struct V557Generator::Impl {
    // Member order keeps history and destinations alive while synthesizer retires its leases.
    struct Eye {
        ComPtr<ID3D12Resource> output;
        std::shared_ptr<xrfg::D3D12SwapchainHistory> history;
        xrfg::D3D12FrameSynthesizer synth;
        D3D12_RESOURCE_DESC desc{};
    };
    ComPtr<ID3D12Device> device;
    SynthesisQueue routing;
    ComPtr<ID3D12Fence> fence,consumer;
    uint64_t value{},consumerValue{},bytes{};
    std::shared_ptr<void> lease;
    std::shared_ptr<const Inputs> inputs[2];
    std::array<Eye,2> eyes;
    Settings settings;
    bool timing{},isolated{},failed{},unsafe{},primed{};
    bool Ready() const {return !unsafe && Done(fence.Get(),value);}
    bool Idle() const {return Ready() && Done(consumer.Get(),consumerValue);}
    bool Check(HRESULT hr,const char* stage) {
        if(SUCCEEDED(hr))return true;
        Log("[framegen-v557] %s failed: 0x%08X\n",stage,unsigned(hr));failed=true;return false;
    }
    bool Signal() {
        if(!Check(routing.Queue()->Signal(fence.Get(),value+1),"completion Signal")) {unsafe=true;return false;}
        ++value;return true;
    }
    void Poll() {
        if(!Ready())return;
        lease.reset();inputs[0].reset();inputs[1].reset();
        double active=0,span=0;bool timed=true;
        for(auto& eye:eyes){xrfg::D3D12NvidiaGpuTiming t{};
            if(eye.synth.consume_nvidia_gpu_timing(&t)!=S_OK){timed=false;continue;}
            active+=double(t.pack_microseconds+t.composition_microseconds)/1000.;
            span+=double(t.total_microseconds)/1000.;}
        if(timed && span>0)OnGeneration(active,span);
    }
};
V557Generator::V557Generator():impl(std::make_unique<Impl>()){}
V557Generator::~V557Generator(){if(impl && !impl->Idle()){Log("[framegen-v557] retaining live GPU resources\n");impl.release();}}
bool V557Generator::Failed() const{return !impl || impl->failed;}
bool V557Generator::Idle() const{return !impl || impl->Idle();}
bool V557Generator::OutputReady() const{return impl && impl->value && impl->Ready();}
bool V557Generator::CanPrepare() const{return impl && !impl->failed && impl->routing.Private() && impl->Ready();}
void V557Generator::Poll(){if(impl)impl->Poll();}
uint64_t V557Generator::Bytes() const{return impl?impl->bytes:0;}
ID3D12Resource* V557Generator::Output(unsigned eye) const{return impl && eye<2?impl->eyes[eye].output.Get():nullptr;}
void V557Generator::ConsumerFence(ID3D12Fence* f,uint64_t v){if(impl){impl->consumer=f;impl->consumerValue=v;}}
bool V557Generator::JoinOutput(ID3D12CommandQueue* consumer){
    if(!impl || !impl->value)return false;
    if(impl->routing.JoinOutput(impl->fence.Get(),impl->value,consumer))return true;
    impl->failed=impl->unsafe=true;flight::Count(flight::Event::SynthesisSyncFailed);return false;
}
void V557Generator::Shutdown(){
    if(!impl)return;
    const auto deadline=GetTickCount64()+2000;
    while(!impl->Idle() && !impl->unsafe && GetTickCount64()<deadline)Sleep(1);
    if(impl->Idle())impl.reset();
    else {Log("[framegen-v557] bounded shutdown retained unfinished resources\n");impl.release();}
}
bool V557Generator::Compatible(ID3D12Device* device,ID3D12CommandQueue* queue,ID3D12Resource* const colors[2],const Settings& settings) const {
    if(!impl || impl->device.Get()!=device || impl->routing.Producer()!=queue ||
       impl->isolated!=refresh::Targeted() || impl->timing!=(MetricsEnabled() || flight::Enabled()) || impl->settings.quality!=settings.quality || impl->settings.flowScale!=settings.flowScale)return false;
    for(unsigned e=0;e<2;++e){if(!colors[e])return false;const auto d=colors[e]->GetDesc();const auto& old=impl->eyes[e].desc;
        if(d.Width!=old.Width || d.Height!=old.Height || d.Format!=old.Format || d.Flags!=old.Flags ||
           d.Dimension!=old.Dimension || d.MipLevels!=old.MipLevels || d.DepthOrArraySize!=old.DepthOrArraySize ||
           d.SampleDesc.Count!=old.SampleDesc.Count || d.SampleDesc.Quality!=old.SampleDesc.Quality || d.Layout!=old.Layout || d.Alignment!=old.Alignment)return false;}
    return true;
}
bool V557Generator::Create(ID3D12Device* device,ID3D12CommandQueue* queue,ID3D12Resource* const colors[2],const Settings& settings) {
    auto& p=*impl;p.device=device;p.settings=settings;p.isolated=refresh::Targeted();p.timing=MetricsEnabled() || flight::Enabled();
    if(!p.routing.Initialize(device,queue,p.isolated)) {p.failed=true;return false;}
    if(p.isolated && !p.routing.Private())flight::Count(flight::Event::SynthesisQueueFallback);
    if(!p.Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&p.fence)),"CreateFence"))return false;
    xrfg::D3D12NvidiaOpticalFlowOptions options;
    options.preset=settings.quality==0?xrfg::D3D12NvidiaPerformancePreset::fast:settings.quality==1?xrfg::D3D12NvidiaPerformancePreset::medium:xrfg::D3D12NvidiaPerformancePreset::slow;
    options.input_scale=settings.flowScale==100?xrfg::D3D12OpticalFlowInputScale::full:settings.flowScale==75?xrfg::D3D12OpticalFlowInputScale::three_quarter:xrfg::D3D12OpticalFlowInputScale::half;
    options.hybrid=false;options.bidirectional=false;
    for(unsigned e=0;e<2;++e){auto& eye=p.eyes[e];eye.desc=colors[e]->GetDesc();
        if(eye.desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || eye.desc.DepthOrArraySize!=1 || eye.desc.MipLevels!=1 || eye.desc.SampleDesc.Count!=1){p.failed=true;return false;}
        auto d=eye.desc;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        if(!p.Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_SOURCE,nullptr,IID_PPV_ARGS(&eye.output)),"destination"))return false;
        // History and destinations; internal reconstruction allocations counted by Bytes().
        p.bytes+=device->GetResourceAllocationInfo(0,1,&d).SizeInBytes;
        p.bytes+=device->GetResourceAllocationInfo(0,1,&eye.desc).SizeInBytes*3;
        eye.history=std::make_shared<xrfg::D3D12SwapchainHistory>();ID3D12Resource* src[]{colors[e]};
        if(!p.Check(eye.history->initialize(device,p.routing.Queue(),src,D3D12_RESOURCE_STATE_COPY_SOURCE),"history initialize"))return false;
        // Only the prime writes B here. Steady pairs write S; native Presenter keeps the real endpoint.
        ID3D12Resource* cur[]{eye.output.Get()};ID3D12Resource* out[]{eye.output.Get()};
        // Initialization can submit OFA registration; on failure retain rather than free unknown GPU work.
        if(!p.Check(eye.synth.initialize(device,p.routing.Queue(),eye.history,cur,out,ViewFormat(d.Format),D3D12_RESOURCE_STATE_COPY_SOURCE,
            xrfg::D3D12OpticalFlowBackend::nvidia,options,p.timing,true),"synth initialize")){p.unsafe=true;return false;}
        p.bytes+=eye.synth.allocated_bytes();
    }
    Log("[framegen-v557] reconstruction initialized: OFA quality=%d scale=%d; current-camera output; %s queue; real-output-copy=0; no external layer\n",settings.quality,settings.flowScale,p.routing.Private()?"private":"application");
    return p.Signal();
}
bool V557Generator::Generate(ID3D12Resource* const colors[2],const EyePair (&cameras)[2],bool reset,bool* interpolated,
                            ID3D12Fence* ready,uint64_t readyValue,std::shared_ptr<void> lease,const std::shared_ptr<const Inputs> (&inputs)[2]) {
    auto& p=*impl;if(interpolated)*interpolated=false;
    if(p.failed)return false;
    if(!p.Ready()){flight::Count(flight::Event::GeneratorBusy);return false;}
    p.Poll();
    if(!p.routing.Begin(ready,readyValue,nullptr,0)){p.failed=p.unsafe=true;return false;}
    p.lease=std::move(lease);p.inputs[0]=inputs[0];p.inputs[1]=inputs[1];
    for(const auto& in:inputs)if(in->readyFence && in->fenceValue && !p.routing.WaitInput(in->readyFence.Get(),in->fenceValue)){p.failed=p.unsafe=true;return false;}
    flight::Count(p.routing.Private()?flight::Event::SynthesisPrivate:flight::Event::SynthesisShared);
    if(p.routing.Private() && (!ready || !readyValue))flight::Count(flight::Event::SynthesisInputFallback);
    CaptureFrame(p.device.Get(),p.routing.Queue(),colors,inputs,cameras);
    // Packing/OFA can overlap the previous display; the component waits immediately
    // before touching either output, whose external state remains COPY_SOURCE.
    if(p.routing.Private() && p.consumerValue)flight::Count(flight::Event::SynthesisConsumerDeferred);
    reset=reset || !p.primed;
    for(unsigned e=0;e<2;++e){auto& eye=p.eyes[e];
        eye.synth.output_consumer(p.consumer.Get(),p.consumerValue);
        if(reset && !p.Check(eye.synth.retire_previous(),"retire history")){p.Signal();return false;}
        xrfg::D3D12HistoryCaptureTicket capture{};xrfg::D3D12FrameSynthesisTicket generated{};
        const auto view=View(cameras[e].current,eye.desc);
        if(!p.Check(eye.history->capture_source(colors[e],&capture),"capture") ||
           !p.Check(eye.history->commit(capture),"commit")){p.Signal();return false;}
        const auto hr=reset?eye.synth.submit_prime(capture,{&view,1},0,&generated):
            eye.synth.submit_pair(capture,{&view,1},{&view,1},0,0,&generated);
        if(!p.Check(hr,reset?"prime":"pair")){
            // A failed OFA call may have submitted work outside the direct queue.
            p.unsafe=true;return false;
        }
    }
    if(!p.Signal())return false;
    p.primed=true;if(interpolated)*interpolated=!reset;return true;
}
}
#endif
