#include "Core/VrCoreShared.hpp"
#include "Hooks/Hook.hpp"
#include "Hooks/PhotoModeCapture.hpp"
#include "Hooks/Trampoline.hpp"
#include "Utils/AobScanner.hpp"
#include "Utils/DebugGate.hpp"
#include "Utils/MemorySafe.hpp"

#include <MinHook.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>

extern "C" UINT GetForcedWindowWidth();
extern "C" UINT GetForcedWindowHeight();

extern "C" {
__declspec(dllexport) std::atomic<uint64_t> CyberpunkVR_PhotoCaptureSizeOverrides{};
__declspec(dllexport) std::atomic<uint64_t> CyberpunkVR_PhotoCaptureClampOverrides{};
void* CvrPhotoClampOriginal{};
void CvrPhotoClampDetour();
}

namespace {
struct CaptureSize {
    uint32_t width;
    uint32_t height;
};
using GetCaptureSizeFn = bool(__fastcall*)(uintptr_t capture, CaptureSize* size);
GetCaptureSizeFn original{};
const auto gameBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
constexpr uintptr_t ReadbackClampReturn = 0x20B5BE2;

uint32_t CurrentCaptureType() {
    uintptr_t renderer{}, capture{};
    uint32_t type{};
    // The renderer owns the capture manager. Read it afresh so a saved pointer
    // cannot outlive an engine/session reset. 2.50 job callers at 0x3D6166
    // and 0x5D4BA1 load the capture manager from renderer+0x360; type remains
    // +0x1C4 (0x2469001). Global identity 2549221846/347be984 maps to 0x3BBFB38.
    if (!ReadPtrSafe(gameBase + 0x3BBFB38, &renderer) || !renderer ||
        !ReadPtrSafe(renderer + 0x360, &capture) || !capture ||
        !ReadU32Safe(capture + 0x1C4, &type)) return 0;
    return type;
}

bool SkipReadbackClamp(uint32_t* width, uint32_t* height) {
    // The native screenshot path also fits its CPU image into 3840x2160.
    // VR keeps the actual texture at the launcher size: at 2560x2560 that
    // produced a 2160x2160 PNG containing rows with a 2560-pixel stride.
    // Called only from the readback CALL site, through a register-preserving
    // thunk. Never detour the shared helper: the world-UI mesh picker keeps its
    // window pointer in R8 across that leaf call (RVA2446763). An ordinary C++
    // entry detour clobbered it even when it chose the original function.
    const UINT forcedWidth = GetForcedWindowWidth();
    const UINT forcedHeight = GetForcedWindowHeight();
    if (width && height && cvr::capture::PreserveReadbackSize(
        true, CurrentCaptureType(), *width, *height, forcedWidth, forcedHeight)) {
        CVR_DIAGNOSTIC(CyberpunkVR_PhotoCaptureClampOverrides.fetch_add(1, std::memory_order_relaxed));
        return true;
    }
    return false;
}

bool __fastcall GetCaptureSize(uintptr_t capture, CaptureSize* size) {
    const bool requested = original(capture, size);
    uint8_t active{};
    uint32_t type{};
    if (!requested || !size || !ReadU8Safe(capture + 0x1C0, &active) ||
        !ReadU32Safe(capture + 0x1C4, &type))
        return requested;

    // Native Photo Mode capture (2.50, +20B5AE0) asks for its screenshot preset.
    // The render job (+2469228) retries a resize and skips the frame until that
    // size matches the active render size. SettingsRes / DXGI keep the VR size,
    // so a 1920x1080 request against 2560x2560 never reaches image readback.
    // Use the same dimensions as those overrides, preserving the game's capture,
    // warm-up, encoding and completion callback. Other size queries stay native.
    const UINT width = GetForcedWindowWidth();
    const UINT height = GetForcedWindowHeight();
    if (cvr::capture::OverrideSize(requested, active != 0, type, width, height) &&
        (size->width != width || size->height != height)) {
        size->width = width;
        size->height = height;
        CVR_DIAGNOSTIC(CyberpunkVR_PhotoCaptureSizeOverrides.fetch_add(1, std::memory_order_relaxed));
    }
    return requested;
}

// The private helper's caller keeps R11 (zero) live across its CALL at
// 0x2469022/0x2469228. A normal C++ detour is allowed to destroy it. Preserve
// volatile input registers and XMM0-5 around the entire original+policy call;
// only RAX carries the bool result back. The two native callers consume no
// other helper result. Keeping the wrapper here also avoids changing the
// shared readback-clamp assembly thunk's unrelated contract.
void* MakeCaptureSizeRelay(void* nearAddress) {
    auto* code = static_cast<uint8_t*>(AllocateTrampoline(nearAddress, 256));
    if (!code) return nullptr;
    int pos = 0;
    const uint8_t save[] = {
        0x9c, 0x51, 0x52, 0x41,0x50, 0x41,0x51, 0x41,0x52, 0x41,0x53, 0x55,
        0x48,0x83,0xec,0x60,
        0x0f,0x11,0x04,0x24, 0x0f,0x11,0x4c,0x24,0x10,
        0x0f,0x11,0x54,0x24,0x20, 0x0f,0x11,0x5c,0x24,0x30,
        0x0f,0x11,0x64,0x24,0x40, 0x0f,0x11,0x6c,0x24,0x50,
        0x48,0x89,0xe5, 0x48,0x83,0xe4,0xf0, 0x48,0x83,0xec,0x20
    };
    std::memcpy(code + pos, save, sizeof(save)); pos += sizeof(save);
    WriteMovRaxImm64(code, pos, reinterpret_cast<uintptr_t>(&GetCaptureSize));
    code[pos++] = 0xff; code[pos++] = 0xd0;
    const uint8_t restore[] = {
        0x48,0x89,0xec,
        0x0f,0x10,0x04,0x24, 0x0f,0x10,0x4c,0x24,0x10,
        0x0f,0x10,0x54,0x24,0x20, 0x0f,0x10,0x5c,0x24,0x30,
        0x0f,0x10,0x64,0x24,0x40, 0x0f,0x10,0x6c,0x24,0x50,
        0x48,0x83,0xc4,0x60,
        0x5d, 0x41,0x5b, 0x41,0x5a, 0x41,0x59, 0x41,0x58, 0x5a, 0x59, 0x9d, 0xc3
    };
    std::memcpy(code + pos, restore, sizeof(restore)); pos += sizeof(restore);
    FlushInstructionCache(GetCurrentProcess(), code, pos);
    return code;
}

bool InstallPhotoModeCapture() {
    // Same capture layout and exact-identity preset resolver as 2.31; compiler
    // now keeps the output in R9 rather than R10. Both native caller sites and
    // the full active/inactive control flow were compared before moving this.
    const char* pattern = "\x48\x83\xEC\x28\x80\xB9\xC0\x01\x00\x00\x00\x4C\x8B\xCA\x74\x2B";
    const char* mask = "xxxxxxxxxxxxxxxx";
    void* target = FindPattern("Cyberpunk2077.exe", pattern, mask);
    if (reinterpret_cast<uintptr_t>(target) != gameBase + 0x20B5AE0) return false;
    const char* clampPattern = "\x81\x39\x00\x0F\x00\x00\x77\x08\x81\x3A\x70\x08\x00\x00\x76\x50";
    // Verify the complete entry before using the mapped 2.50 RVA; several unrelated
    // image utilities compare against 3840 too.
    auto* clamp = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr)) + 0x27FEB44;
    auto* call = reinterpret_cast<uint8_t*>(gameBase + ReadbackClampReturn - 5);
    int32_t displacement{};
    std::memcpy(&displacement, call + 1, sizeof(displacement));
    if (std::memcmp(clamp, clampPattern, 16) != 0 || call[0] != 0xE8 ||
        gameBase + ReadbackClampReturn + displacement != reinterpret_cast<uintptr_t>(clamp)) return false;

    // A CALL to this nearby register-neutral relay retains the original return
    // address. The DLL thunk has normal unwind metadata and preserves the leaf
    // register contract around its C++ policy, then tail-calls the real helper.
    auto* relay = static_cast<uint8_t*>(AllocateTrampoline(call, 14));
    if (!relay) return false;
    const auto relative = reinterpret_cast<intptr_t>(relay) - reinterpret_cast<intptr_t>(call + 5);
    if (relative < std::numeric_limits<int32_t>::min() || relative > std::numeric_limits<int32_t>::max()) return false;
    const uint8_t jump[] = {0xff,0x25,0,0,0,0};
    std::memcpy(relay,jump,sizeof(jump));
    const auto thunk = reinterpret_cast<uintptr_t>(&CvrPhotoClampDetour);
    std::memcpy(relay+6,&thunk,sizeof(thunk));
    FlushInstructionCache(GetCurrentProcess(),relay,14);
    CvrPhotoClampOriginal = clamp;
    void* sizeRelay = MakeCaptureSizeRelay(target);
    if (!sizeRelay) return false;
    if (MH_CreateHook(target,sizeRelay,reinterpret_cast<void**>(&original)) != MH_OK) return false;
    if (MH_EnableHook(target) != MH_OK) { MH_RemoveHook(target); return false; }
    uint8_t before[5], after[5] = {0xe8};
    std::memcpy(before,call,sizeof(before));
    const auto callOffset = static_cast<int32_t>(relative);
    std::memcpy(after+1,&callOffset,sizeof(callOffset));
    DWORD protection{};
    if (!VirtualProtect(call,sizeof(after),PAGE_EXECUTE_READWRITE,&protection)) {
        MH_DisableHook(target); MH_RemoveHook(target); return false;
    }
    std::memcpy(call,after,sizeof(after));
    DWORD ignored{};
    if (!VirtualProtect(call,sizeof(after),protection,&ignored)) {
        std::memcpy(call,before,sizeof(before));
        VirtualProtect(call,sizeof(before),protection,&ignored);
        FlushInstructionCache(GetCurrentProcess(),call,sizeof(before));
        MH_DisableHook(target); MH_RemoveHook(target); return false;
    }
    FlushInstructionCache(GetCurrentProcess(),call,sizeof(after));
    Log("[photo-capture] readback-only clamp call installed; native UI clamp untouched\n");
    return true;
}

CVR_HOOK("PhotoModeCapture", ::cvr::hooks::Stage::Boot, 86, InstallPhotoModeCapture);
}

extern "C" bool CvrPhotoClampSkip(uint32_t* width, uint32_t* height) {
    return SkipReadbackClamp(width,height);
}
