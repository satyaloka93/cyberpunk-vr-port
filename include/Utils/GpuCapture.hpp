#pragma once
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <algorithm>
#include "Framegen/Framegen.hpp"
#include "Render/SceneGpuProfile.hpp"
#include "Overlay/ImGuiOverlay.hpp"

extern void Log(const char*, ...);
extern "C" {
extern std::atomic<uint32_t> CyberpunkVR_GpuStageRequest, CyberpunkVR_GpuStageReportSeq;
extern char CyberpunkVR_GpuStageReport[2048];
extern std::atomic<uint32_t> CyberpunkVR_GpuTimelineRequest, CyberpunkVR_GpuTimelineState, CyberpunkVR_GpuTimelineCount;
extern uint64_t CyberpunkVR_GpuTimelineCalib[8], CyberpunkVR_GpuTimelineRecords[16384 * 6];
}
namespace cvr::gpu_capture {
enum Phase { Idle, Countdown, Scene, PortGap, Port, QueueGap, Queue, Done, Failed };
inline std::atomic<int> phase{Idle};
inline std::atomic<unsigned> seconds{0};
inline std::mutex mutex;
inline uint64_t deadline{};
inline uint32_t previousSeq{}, oldGroup{}, oldFilter{};
inline std::atomic<bool> ownsSceneSettings{};
inline std::filesystem::path output;
inline const char* Status() {
    switch (phase.load()) {
    case Countdown: return "Countdown: close F10; keep FG off";
    case Scene: return "Capturing per-eye scene GPU costs";
    case PortGap: case Port: return "Capturing port copies / HUD GPU costs";
    case QueueGap: case Queue: return "Capturing GPU queue timeline";
    case Done: return "Saved: bin/x64/cpvr-gpu-captures";
    case Failed: return "Capture incomplete; see cyberpunkvrport.log";
    default: return "Idle";
    }
}
inline bool Busy() { const int p=phase.load(); return p>=Countdown && p<=Queue; }
inline void Arm() {
    std::lock_guard lock(mutex);
    if(Busy() || cvr::framegen::Enabled()) return;
    if(ownsSceneSettings)return; // previous timed-out scene must finish draining
    deadline=GetTickCount64()+10000;
    seconds=10;
    phase=Countdown;
}
inline void Save(const char* name,const std::string& text) {
    std::ofstream file(output/name,std::ios::binary);
    file.exceptions(std::ios::failbit|std::ios::badbit);
    file << text << '\n';
    file.close();
}
inline void RestoreSceneSettings() {
    if(!ownsSceneSettings)return;
    CyberpunkVR_SceneGpuGroupMode=oldGroup;
    CyberpunkVR_SceneGpuNodeFilter=oldFilter;
    ownsSceneSettings=false;
}
inline void Fail(const char* reason) {
    // Never free in-flight profiler resources. The existing profilers drain them.
    const auto p=phase.load();
    if(p==Scene)CyberpunkVR_SceneGpuRequest=0;
    if(p==Port)CyberpunkVR_GpuStageRequest=0;
    if(p==Queue)CyberpunkVR_GpuTimelineRequest=0;
    if(!cvr::gpu::scene_profile::active.load())RestoreSceneSettings();
    phase=Failed;
    Log("[gpu-capture] incomplete: %s\n",reason);
}
inline bool OtherProbeBusy() {
    const auto q=CyberpunkVR_GpuTimelineState.load();
    return cvr::gpu::scene_profile::active.load() || CyberpunkVR_SceneGpuRequest.load() ||
        CyberpunkVR_GpuStageRequest.load() ||
        std::string(CyberpunkVR_GpuStageReport).find("\"pending\":true")!=std::string::npos ||
        CyberpunkVR_GpuTimelineRequest.load() || q==1 || q==2;
}
// Called after the upstream profiler Poll functions, only on the game Present.
// No GPU work while idle; finite requests, no extra waits or broad debug gate.
inline void Tick() noexcept {
    if(!Busy() && !ownsSceneSettings)return;
    std::unique_lock lock(mutex,std::try_to_lock);
    if(!lock.owns_lock())return;
    try {
        if(phase==Failed) {
            if(!cvr::gpu::scene_profile::active.load())RestoreSceneSettings();
            return;
        }
        const auto now=GetTickCount64();
        if(cvr::framegen::Enabled()){Fail("FG enabled during capture");return;}
        if(phase==Countdown) {
            // Closing F10 starts a full clean ten-second countdown.
            if(OverlayIsVisible())deadline=now+10000;
            seconds=unsigned(now<deadline?(deadline-now+999)/1000:0);
            if(now<deadline)return;
            if(OtherProbeBusy()){Fail("another profiler is active");return;}
            wchar_t exe[32768]{};
            const DWORD length=GetModuleFileNameW(nullptr,exe,32768);
            if(!length || length>=32768){Fail("executable path unavailable");return;}
            output=std::filesystem::path(exe).parent_path()/"cpvr-gpu-captures"/
                (std::to_string(GetCurrentProcessId())+"-"+std::to_string(now));
            std::filesystem::create_directories(output);
            Save("metadata.txt","FG disabled; sequential scene(256 lists), port(32 batches), queue(512 submissions).\n"
                "Per-node totals are sampled GPU work, NOT automatically milliseconds/frame.\n"
                "Queue initialization and capture overhead are not normal gameplay cost.\n"
                "Scene/port/queue stages are separate time windows. Preserve camera and workload.");
            oldGroup=CyberpunkVR_SceneGpuGroupMode.load();
            oldFilter=CyberpunkVR_SceneGpuNodeFilter.load();
            ownsSceneSettings=true;
            CyberpunkVR_SceneGpuGroupMode=1;
            CyberpunkVR_SceneGpuNodeFilter=0;
            previousSeq=CyberpunkVR_SceneGpuReportSeq.load();
            CyberpunkVR_SceneGpuRequest=256;
            phase=Scene;deadline=now+15000;
            Log("[gpu-capture] scene start; output=%ls\n",output.c_str());
            return;
        }
        if(OverlayIsVisible()){Fail("F10 reopened during capture");return;}
        if(phase==Scene && CyberpunkVR_SceneGpuReportSeq.load()!=previousSeq &&
           !CyberpunkVR_SceneGpuRequest.load() && !cvr::gpu::scene_profile::active.load()) {
            const std::string report=CyberpunkVR_SceneGpuReport;
            Save("scene.json",report);
            RestoreSceneSettings();
            if(report.find("\"groups\":[]")!=std::string::npos ||
               report.find("\"error\"")!=std::string::npos) {
                Fail("scene capture returned no usable groups; report preserved");return;
            }
            phase=PortGap;deadline=now+2000;return;
        }
        if(phase==PortGap && now>=deadline) {
            previousSeq=CyberpunkVR_GpuStageReportSeq.load();
            CyberpunkVR_GpuStageRequest=32;
            phase=Port;deadline=now+15000;
            Log("[gpu-capture] port stages start\n");return;
        }
        if(phase==Port && CyberpunkVR_GpuStageReportSeq.load()!=previousSeq &&
           !CyberpunkVR_GpuStageRequest.load() &&
           std::string(CyberpunkVR_GpuStageReport).find("\"pending\":false")!=std::string::npos) {
            Save("port.json",CyberpunkVR_GpuStageReport);
            phase=QueueGap;deadline=now+2000;return;
        }
        if(phase==QueueGap && now>=deadline) {
            CyberpunkVR_GpuTimelineRequest=512;
            phase=Queue;deadline=now+20000;
            Log("[gpu-capture] queue start (initialization may hitch)\n");return;
        }
        if(phase==Queue && !CyberpunkVR_GpuTimelineRequest.load()) {
            const auto state=CyberpunkVR_GpuTimelineState.load();
            if(state==4){Fail("queue profiler initialization failed");return;}
            if(state==3) {
                const auto count=std::min(CyberpunkVR_GpuTimelineCount.load(),16384u);
                std::ostringstream json;
                json<<"{\"calibration\":[";
                for(int i=0;i<8;++i){if(i)json<<',';json<<CyberpunkVR_GpuTimelineCalib[i];}
                json<<"],\"records\":[";
                for(unsigned i=0;i<count;++i){
                    if(i)json<<',';json<<'[';
                    for(int j=0;j<6;++j){if(j)json<<',';json<<CyberpunkVR_GpuTimelineRecords[i*6+j];}
                    json<<']';
                }
                json<<"]}";
                Save("queue.json",json.str());
                phase=Done;
                Log("[gpu-capture] saved all reports: %ls\n",output.c_str());
                return;
            }
        }
        if(now>=deadline && phase!=PortGap && phase!=QueueGap)Fail("profiler timed out; partial files retained");
    } catch(...) {Fail("report allocation or file write failed");}
}
}
