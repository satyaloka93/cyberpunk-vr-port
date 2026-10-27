#pragma once
#include <array>
#include <cstdint>
#include <mutex>

namespace cvr::stereo::cascade_freshness {
struct Context {
    uintptr_t renderer{}, owner{};
    uint32_t frame{};
    bool Valid() const { return renderer && owner; }
    bool operator==(const Context&) const = default;
};
// The renderer advances its frame for each eye in the sequential port. Both
// same-frame dispatches and the immediately following MAIN frame are valid;
// arbitrary older captures are not. Unsigned subtraction handles wraparound.
inline bool Adjacent(const Context& source,const Context& destination) {
    return source.Valid() && destination.Valid() && source.renderer==destination.renderer &&
        source.owner==destination.owner && uint32_t(destination.frame-source.frame)<=1;
}
struct Policy {
    Context source{},main{};
    uint64_t generation{},accepted{},rejected{},confirmed{},serial{};
    bool haveMain{},sampled{},samplingDecided{};
    std::array<uint64_t,8> pending{},ready{},tickets{},cameras{};
    void Begin(const Context& context) {
        source=context;main={};haveMain=false;sampled=false;samplingDecided=false;pending={};ready={};cameras={};
        if(++generation==0)++generation;
    }
    bool Source(const Context& context) const {return source.Valid() && context==source;}
    bool Main(const Context& context) {
        if(!Adjacent(source,context) || (haveMain && !(main==context))) {++rejected;return false;}
        main=context;haveMain=true;++accepted;return true;
    }
    uint64_t Stage(const Context& context,unsigned index) {
        if(index>=ready.size())return 0;
        ready[index]=0;pending[index]=Source(context)?generation:0;
        if(++serial==0)++serial;
        tickets[index]=serial;
        return pending[index]?serial:0;
    }
    void Uploaded(const Context& context,unsigned index,uint64_t ticket) {
        if(index<cameras.size() && Source(context))cameras[index]=generation;
        if(index<ready.size() && ticket && tickets[index]==ticket && Source(context) && pending[index]==generation) {
            ready[index]=generation;++confirmed;
        }
    }
    bool StartSampling(const Context& context) {
        if(!Main(context))return false;
        if(samplingDecided && !sampled){++rejected;return false;}
        return true;
    }
    void FinishSampling(bool success) {
        // An incomplete first MAIN upload selects native fallback for the
        // whole generation; later source completion must not promote it.
        if(!samplingDecided){samplingDecided=true;sampled=success;}
    }
    bool Bundle(const Context& context,unsigned count,bool needRecords) {
        if(!count || count>4 || !generation){++rejected;return false;}
        for(unsigned i=0;i<count;++i) {
            if(cameras[i]!=generation || (needRecords && ready[i]!=generation)){++rejected;return false;}
        }
        return Main(context);
    }
    bool Record(const Context& context,unsigned index) {
        if(index>=ready.size() || !generation || ready[index]!=generation){++rejected;return false;}
        return Main(context);
    }
};
struct SamplingAttempt {
    Policy& policy;
    bool success{};
    ~SamplingAttempt(){policy.FinishSampling(success);}
    void Commit(){success=true;policy.FinishSampling(true);}
};
inline Policy policy;
// Also protects the record/camera/sampling payloads in the two translation units.
// Recursive because a cascade dispatch calls the native uploader synchronously.
inline std::recursive_mutex mutex;
inline thread_local std::array<uint64_t,8> stagedRecords{};
Context ReadContext(uintptr_t view);
// Caller holds mutex. Logging is bounded independently of runtime diagnostic switches.
void Report(const char* operation,const Context& current,bool accepted);
}
