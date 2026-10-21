#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
extern void Log(const char*, ...);
namespace cvr::audit {
// CPU wall spans only: no GPU queries, waits, hooks, worker or resource copies.
enum Stage { Present, Overlay, OverlayFence, Dlss, XrWait, XrEnd, XrFence, Capture, Dxgi, StageCount };
inline constexpr const char* names[]{"present","overlay","overlayFence","dlssSubmit","xrWait","xrEnd","xrFence","capture","dxgiPresent"};
struct Bucket { std::atomic<uint64_t> count{},us{},maximum{}; };
inline Bucket buckets[StageCount];
enum Event { InputCall, NoPose, NoFrame, WrongList, NoCamera, NoSlot, CopyFailed,
    ReadyMain, ReadyVrcam, AcquireMain, AcquireVrcam, AcquireMiss, MissingPair,
    NoStereo, NoLease, GenerateFail, Generated, TagEntry, TagDisabled, NullTags, TagCount, NullCommand, ConstantsEntry, EvaluateEntry, EventCount };
inline std::atomic<uint64_t> events[EventCount]{};
inline constexpr const char* eventNames[]{"inputCall","noPose","noFrame","wrongList","noCamera","noSlot","copyFailed",
    "readyMain","readyVrcam","acquireMain","acquireVrcam","acquireMiss","missingPair","noStereo","noLease","generateFail","generated",
    "tagEntry","tagDisabled","nullTags","invalidTagCount","nullCommand","constantsEntry","evaluateEntry"};
inline void Count(Event e) { events[e].fetch_add(1,std::memory_order_relaxed); }
inline uint64_t Ticks(){LARGE_INTEGER x{};QueryPerformanceCounter(&x);return x.QuadPart;}
inline double Scale(){static const double s=[]{LARGE_INTEGER x{};QueryPerformanceFrequency(&x);return 1000000.0/x.QuadPart;}();return s;}
inline void Finish(Stage stage,uint64_t start){auto& b=buckets[stage];const auto us=uint64_t((Ticks()-start)*Scale());
        b.us.fetch_add(us,std::memory_order_relaxed);b.count.fetch_add(1,std::memory_order_relaxed);
        auto max=b.maximum.load(std::memory_order_relaxed);
        while(max<us && !b.maximum.compare_exchange_weak(max,us,std::memory_order_relaxed)){} }
struct Span {
    Stage stage; uint64_t start;
    explicit Span(Stage s):stage(s),start(Ticks()){}
    ~Span(){Finish(stage,start);}
};
// Present-thread only. Non-resetting counters avoid losses/races at window boundaries.
inline void Flush(){
    static std::atomic_flag busy=ATOMIC_FLAG_INIT;
    if(busy.test_and_set(std::memory_order_acquire))return;
    struct Unlock {std::atomic_flag& flag;~Unlock(){flag.clear(std::memory_order_release);}} unlock{busy};
    static uint64_t next{},last{},counts[StageCount]{},totals[StageCount]{},previous[EventCount]{};
    const auto now=GetTickCount64();if(now<next)return;next=now+2000;
    if(!last){last=now;return;}
    const double seconds=(now-last)/1000.0;last=now;
    for(unsigned i=0;i<StageCount;++i){auto& b=buckets[i];const auto n=b.count.load(),u=b.us.load();
        const auto dn=n-counts[i],du=u-totals[i];counts[i]=n;totals[i]=u;
        Log("[run-audit] stage=%s seconds=%.3f calls=%llu rate=%.2f cpu_avg_ms=%.3f lifetime_max_ms=%.3f\n",
            names[i],seconds,dn,dn/seconds,dn?du/(1000.0*dn):0,b.maximum.load()/1000.0);}
    for(unsigned i=0;i<EventCount;++i){const auto n=events[i].load();const auto d=n-previous[i];previous[i]=n;
        if(d)Log("[fg-audit] event=%s count=%llu seconds=%.3f\n",eventNames[i],d,seconds);}
}
}
