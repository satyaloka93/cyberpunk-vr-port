#pragma once
#include <cstdint>
#include <limits>

namespace cvr::detail {
enum class AutoGrassStatus : unsigned { Ready, MissingContext, MissingState, MissingBuffer };
struct AutoGrassReadiness {
    AutoGrassStatus status=AutoGrassStatus::MissingContext;
    uintptr_t state{};
    uint32_t buffer{};
    explicit operator bool() const {return status==AutoGrassStatus::Ready;}
};

// CP2077 2.50 PrepareAutoSpawnOnTerrain (7F6B98, mapped from 77B638)
// retains workContext+20 -> owner+98. Its AutoGrass branch reads count+81C
// at7F6F24 and passes state+830 to419C20 at7F6F76/7F6F7E, without a
// null-handle check. During save replacement the count can already be
// nonzero while this buffer is still zero. 2.31 used count+79C/buffer+7B0.
template<class ReadPointer,class ReadHandle>
AutoGrassReadiness InspectAutoGrassInputs(uintptr_t workContext,ReadPointer readPointer,ReadHandle readHandle) {
    AutoGrassReadiness out;
    uintptr_t owner{};
    if(!workContext || !readPointer(workContext+0x20,&owner) || !owner)return out;
    out.status=AutoGrassStatus::MissingState;
    if(!readPointer(owner+0x98,&out.state) || !out.state)return out;
    out.status=AutoGrassStatus::MissingBuffer;
    if(!readHandle(out.state+0x830,&out.buffer) || !out.buffer ||
       out.buffer==std::numeric_limits<uint32_t>::max())return out;
    out.status=AutoGrassStatus::Ready;
    return out;
}
}
