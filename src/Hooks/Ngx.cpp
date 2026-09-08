// NGX DLSS EvaluateFeature read-only hook.
//
// Patches nvngx_dlss.dll!NVSDK_NGX_D3D12_EvaluateFeature with a JMP that
// captures the engine's per-frame motion-vector / depth resources and the
// scale / reset NGX parameters. The original function is then re-entered via
// a small trampoline (saved displaced bytes + JMP back). No NGX state is
// modified — DLSS sees the same call it always saw.
//
// The NGX parameter struct exposes a stable vtable on its v-table-0 slot, but
// nvngx_dlss.dll's INTERNAL parameter type is opaque. Fortunately the DLSS
// SDK is open: NVSDK_NGX_Parameter is a thin wrapper around setters/getters
// that the game side already populates. We do NOT call NGX SDK API here —
// instead we walk a small slot of well-known string→pointer entries that the
// in-memory parameter object carries. We confirm fields by their KNOWN value
// patterns (resource pointer with COM vtable, float scale, int reset).
//
// This file deliberately avoids depending on the NGX SDK headers — the only
// constant we need is the function signature shape:
//
//   NVSDK_NGX_Result NVSDK_NGX_D3D12_EvaluateFeature(
//       ID3D12GraphicsCommandList* cmdList,
//       const NVSDK_NGX_Handle* featureHandle,
//       const NVSDK_NGX_Parameter* params,
//       PFN_NVSDK_NGX_ProgressCallback progress)
//
// rcx=cmdList, rdx=featureHandle, r8=params, r9=progress.

#include "Hooks/Ngx.hpp"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <atomic>
#include <array>
#include <algorithm>
#include <MinHook.h>

#include "Core/VrCoreShared.hpp"

extern void Log(const char* fmt, ...);
extern volatile int g_verboseLog;
extern char g_liveControlPath[MAX_PATH];
extern "C" int CyberpunkVR_GetDlssEvalViewKey(unsigned long long* out);
extern "C" int CyberpunkVR_GetSlEvaluateCurrentContext(unsigned long long* sequence, int* side);
extern "C" uint64_t CyberpunkVR_VrcamCtxKey();

namespace {

std::atomic<uint8_t*> g_trampolineEntry{nullptr};

// Captured engine resources, refcounted (AddRef'd into the slots, Release'd
// when overwritten). Mutex protects the lifetime swap; the reads use AddRef
// under lock so the consumer can Release outside our scope safely.
std::mutex g_captureMutex;
ID3D12Resource* g_mvRes = nullptr;
ID3D12Resource* g_depthRes = nullptr;
// The pre-upscale colour the game hands DLSS SR. Captured for the direct-DLSSNR
// experiment, which runs the model at RENDER resolution rather than output resolution.
ID3D12Resource* g_colorRes = nullptr;
std::atomic<unsigned int> g_colorWidth{0}, g_colorHeight{0}, g_colorFormat{0};
std::atomic<float> g_mvScaleX{1.0f};
std::atomic<float> g_mvScaleY{1.0f};
std::atomic<int> g_resetFlag{0};
std::atomic<unsigned int> g_mvWidth{0};
std::atomic<unsigned int> g_mvHeight{0};
std::atomic<unsigned int> g_mvFormat{0};
std::atomic<unsigned int> g_evalCount{0};

// ---- feature-18 lifecycle/evaluation census ---------------------------------------------------
//
// The generic addon hooks the game's ordinary NGX/Streamline evaluate and injects feature 18 from
// inside that call. Hooking those already-detoured entry points would either displace the addon or
// recurse through it. nvngx_dlssnr.dll's own D3D12 exports are one layer lower and, in the tested
// 310.8 runtime, remain pristine. MinHook relocates their RIP-relative prologues correctly; the old
// hand-written 16-byte trampoline above cannot. We still verify every original prologue before
// asking MinHook to touch it and fail closed if another detour got there first.
using NrCreateFn = uint32_t(__fastcall*)(ID3D12GraphicsCommandList*, uint32_t, void*, void**);
using NrEvaluateFn = uint32_t(__fastcall*)(ID3D12GraphicsCommandList*, const void*, const void*, void*);
using NrReleaseFn = uint32_t(__fastcall*)(const void*);
NrCreateFn   g_nrCreateOrig = nullptr;
NrEvaluateFn g_nrEvaluateOrig = nullptr;
NrReleaseFn  g_nrReleaseOrig = nullptr;
// This forwarding implementation is permanently disabled: its CALL through MinHook's trampoline
// changes the return address seen by signed runtime 310.8 and makes feature 18 return 0xBAD00002.
// A later UEVR experiment proved that a prehook which JMPs to the trampoline can preserve the
// addon's caller address; do not re-enable this CALL-based implementation as a shortcut.
std::atomic<int> g_nrDiagState{0};
std::atomic<uint64_t> g_nrCreates[3];
std::atomic<uint64_t> g_nrEvals[3];
std::atomic<uint64_t> g_nrMenuEvals[3];
std::atomic<uint64_t> g_nrBadResults[3];
std::atomic<uint64_t> g_nrReleases[3];
std::atomic<uint64_t> g_nrSharedHandleEvals{0};
std::atomic<uint64_t> g_nrRecursiveEvals{0};
std::atomic<uint64_t> g_nrTotalEvalUs{0};
std::atomic<uint64_t> g_nrMaxEvalUs{0};
std::atomic<uint64_t> g_nrLastEvalTick[3];
std::atomic<uintptr_t> g_nrLastHandle[3];
std::atomic<uintptr_t> g_nrLastParams[3];
std::atomic<uint32_t> g_nrLastResult[3];
std::atomic<uint64_t> g_nrParameterSamples[3];
std::atomic<uint64_t> g_nrParameterGetFailures[3];
std::atomic<uint64_t> g_nrParameterValidMask[3];
std::atomic<uint64_t> g_nrParameterFingerprint[3];
std::atomic<float> g_nrIntensity[3], g_nrLocalTone[3], g_nrLocalStructure[3], g_nrSkinStructure[3];
std::atomic<float> g_nrMotionScaleX[3], g_nrMotionScaleY[3];
std::atomic<int> g_nrPreset[3], g_nrStyle[3], g_nrAutoMask[3], g_nrUiCorrection[3];
std::atomic<int> g_nrDepthInverted[3], g_nrReset[3];
std::atomic<uint32_t> g_nrInputWidth[3], g_nrInputHeight[3], g_nrOutputWidth[3], g_nrOutputHeight[3];
std::atomic<uintptr_t> g_nrColorResource[3], g_nrOutputResource[3];
std::atomic<uintptr_t> g_nrMotionResource[3], g_nrDepthResource[3];
std::atomic<uint64_t> g_nrTotalEvals{0};
uint64_t g_nrQpcFrequency = 0;
thread_local bool t_nrDiagInEvaluate = false;

int NrDiagSide(uint64_t* keyOut = nullptr) {
    unsigned long long key = 0;
    if (!CyberpunkVR_GetDlssEvalViewKey(&key)) {
        if (keyOut) *keyOut = 0;
        return 2;
    }
    if (keyOut) *keyOut = static_cast<uint64_t>(key);
    if (key == 0) return 0;
    if (key == CyberpunkVR_VrcamCtxKey()) return 1;
    return 2;
}

const char* NrDiagSideName(int side) {
    return side == 0 ? "MAIN" : side == 1 ? "VRCAM" : "OTHER";
}

void AtomicMax(std::atomic<uint64_t>& dst, uint64_t value) {
    uint64_t cur = dst.load(std::memory_order_relaxed);
    while (cur < value && !dst.compare_exchange_weak(cur, value, std::memory_order_relaxed)) {}
}

// Official NVSDK_NGX_Parameter ABI (NVIDIA/DLSS nvsdk_ngx_params.h): eight Set overloads,
// followed by Get ULL/F/D/UI/I/D3D11/D3D12/void*. Calling only Get keeps this diagnostic strictly
// read-only. Each call is SEH-contained because the feature-18 parameter object is supplied by a
// closed addon; an ABI mismatch disables this sample rather than taking down the render thread.
bool NrParamGetFloat(const void* params, const char* name, float* out) {
    if (!params || !name || !out) return false;
    uint32_t result = 0xFFFFFFFFu;
    __try {
        void** vt = *reinterpret_cast<void***>(const_cast<void*>(params));
        using Fn = uint32_t(__fastcall*)(const void*, const char*, float*);
        result = reinterpret_cast<Fn>(vt[9])(params, name, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return (result & 0x80000000u) == 0;
}

bool NrParamGetUInt(const void* params, const char* name, uint32_t* out) {
    if (!params || !name || !out) return false;
    uint32_t result = 0xFFFFFFFFu;
    __try {
        void** vt = *reinterpret_cast<void***>(const_cast<void*>(params));
        using Fn = uint32_t(__fastcall*)(const void*, const char*, uint32_t*);
        result = reinterpret_cast<Fn>(vt[11])(params, name, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return (result & 0x80000000u) == 0;
}

bool NrParamGetInt(const void* params, const char* name, int* out) {
    if (!params || !name || !out) return false;
    uint32_t result = 0xFFFFFFFFu;
    __try {
        void** vt = *reinterpret_cast<void***>(const_cast<void*>(params));
        using Fn = uint32_t(__fastcall*)(const void*, const char*, int*);
        result = reinterpret_cast<Fn>(vt[12])(params, name, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return (result & 0x80000000u) == 0;
}

bool NrParamGetResource(const void* params, const char* name, ID3D12Resource** out) {
    if (!params || !name || !out) return false;
    uint32_t result = 0xFFFFFFFFu;
    __try {
        void** vt = *reinterpret_cast<void***>(const_cast<void*>(params));
        using Fn = uint32_t(__fastcall*)(const void*, const char*, ID3D12Resource**);
        result = reinterpret_cast<Fn>(vt[14])(params, name, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return (result & 0x80000000u) == 0;
}

enum NrParamBit : uint64_t {
    kNrIntensity      = 1ull << 0,  kNrLocalTone       = 1ull << 1,
    kNrLocalStructure = 1ull << 2,  kNrSkinStructure   = 1ull << 3,
    kNrMotionX        = 1ull << 4,  kNrMotionY         = 1ull << 5,
    kNrPreset         = 1ull << 6,  kNrStyle           = 1ull << 7,
    kNrAutoMask       = 1ull << 8,  kNrUiCorrection    = 1ull << 9,
    kNrDepthInverted  = 1ull << 10, kNrReset           = 1ull << 11,
    kNrInputWidth     = 1ull << 12, kNrInputHeight     = 1ull << 13,
    kNrOutputWidth    = 1ull << 14, kNrOutputHeight    = 1ull << 15,
    kNrColor          = 1ull << 16, kNrOutput          = 1ull << 17,
    kNrMotion         = 1ull << 18, kNrDepth           = 1ull << 19,
};

struct NrParameterValues {
    uint64_t mask = 0;
    float intensity = 0, localTone = 0, localStructure = 0, skinStructure = 0;
    float motionX = 0, motionY = 0;
    int preset = 0, style = 0, autoMask = 0, uiCorrection = 0, depthInverted = 0, reset = 0;
    uint32_t inputWidth = 0, inputHeight = 0, outputWidth = 0, outputHeight = 0;
    ID3D12Resource* color = nullptr;
    ID3D12Resource* output = nullptr;
    ID3D12Resource* motion = nullptr;
    ID3D12Resource* depth = nullptr;
};

uint64_t NrParameterFingerprint(const NrParameterValues& v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    uint64_t h = 1469598103934665603ull;
    for (size_t i = sizeof(v.mask); i < sizeof(v); ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h ^ v.mask;
}

void SampleNrParameters(const void* params, int side, uint64_t key, bool menu) {
    NrParameterValues v{};
#define NR_GET(call, bit) do { if (call) v.mask |= (bit); } while (0)
    NR_GET(NrParamGetFloat(params, "DLSSNR.Intensity", &v.intensity), kNrIntensity);
    NR_GET(NrParamGetFloat(params, "DLSSNR.LocalToneStrength", &v.localTone), kNrLocalTone);
    NR_GET(NrParamGetFloat(params, "DLSSNR.LocalStructureStrength", &v.localStructure), kNrLocalStructure);
    NR_GET(NrParamGetFloat(params, "DLSSNR.SkinStructureStrength", &v.skinStructure), kNrSkinStructure);
    NR_GET(NrParamGetFloat(params, "DLSSNR.MVecScaleX", &v.motionX), kNrMotionX);
    NR_GET(NrParamGetFloat(params, "DLSSNR.MVecScaleY", &v.motionY), kNrMotionY);
    NR_GET(NrParamGetInt(params, "DLSSNR.Hint.Render.Preset", &v.preset), kNrPreset);
    NR_GET(NrParamGetInt(params, "DLSSNR.Style", &v.style), kNrStyle);
    NR_GET(NrParamGetInt(params, "DLSSNR.UseAutoMask", &v.autoMask), kNrAutoMask);
    NR_GET(NrParamGetInt(params, "DLSSNR.UICorrection", &v.uiCorrection), kNrUiCorrection);
    NR_GET(NrParamGetInt(params, "DLSSNR.DepthInverted", &v.depthInverted), kNrDepthInverted);
    NR_GET(NrParamGetInt(params, "DLSSNR.Reset", &v.reset), kNrReset);
    NR_GET(NrParamGetUInt(params, "DLSSNR.InputWidth", &v.inputWidth), kNrInputWidth);
    NR_GET(NrParamGetUInt(params, "DLSSNR.InputHeight", &v.inputHeight), kNrInputHeight);
    NR_GET(NrParamGetUInt(params, "DLSSNR.OutputWidth", &v.outputWidth), kNrOutputWidth);
    NR_GET(NrParamGetUInt(params, "DLSSNR.OutputHeight", &v.outputHeight), kNrOutputHeight);
    NR_GET(NrParamGetResource(params, "DLSSNR.Color", &v.color), kNrColor);
    NR_GET(NrParamGetResource(params, "DLSSNR.Output", &v.output), kNrOutput);
    NR_GET(NrParamGetResource(params, "DLSSNR.MVec", &v.motion), kNrMotion);
    NR_GET(NrParamGetResource(params, "DLSSNR.Depth", &v.depth), kNrDepth);
#undef NR_GET

    const uint64_t sample = g_nrParameterSamples[side].fetch_add(1, std::memory_order_relaxed) + 1;
    if (!v.mask) {
        const uint64_t fails = g_nrParameterGetFailures[side].fetch_add(1, std::memory_order_relaxed) + 1;
        if (fails <= 4) Log("[DLSSNR-DIAG][params] view=%s key=0x%llX all getters failed params=%p\n",
                            NrDiagSideName(side), key, params);
        return;
    }

    g_nrParameterValidMask[side].store(v.mask, std::memory_order_relaxed);
    g_nrIntensity[side].store(v.intensity, std::memory_order_relaxed);
    g_nrLocalTone[side].store(v.localTone, std::memory_order_relaxed);
    g_nrLocalStructure[side].store(v.localStructure, std::memory_order_relaxed);
    g_nrSkinStructure[side].store(v.skinStructure, std::memory_order_relaxed);
    g_nrMotionScaleX[side].store(v.motionX, std::memory_order_relaxed);
    g_nrMotionScaleY[side].store(v.motionY, std::memory_order_relaxed);
    g_nrPreset[side].store(v.preset, std::memory_order_relaxed);
    g_nrStyle[side].store(v.style, std::memory_order_relaxed);
    g_nrAutoMask[side].store(v.autoMask, std::memory_order_relaxed);
    g_nrUiCorrection[side].store(v.uiCorrection, std::memory_order_relaxed);
    g_nrDepthInverted[side].store(v.depthInverted, std::memory_order_relaxed);
    g_nrReset[side].store(v.reset, std::memory_order_relaxed);
    g_nrInputWidth[side].store(v.inputWidth, std::memory_order_relaxed);
    g_nrInputHeight[side].store(v.inputHeight, std::memory_order_relaxed);
    g_nrOutputWidth[side].store(v.outputWidth, std::memory_order_relaxed);
    g_nrOutputHeight[side].store(v.outputHeight, std::memory_order_relaxed);
    g_nrColorResource[side].store(reinterpret_cast<uintptr_t>(v.color), std::memory_order_relaxed);
    g_nrOutputResource[side].store(reinterpret_cast<uintptr_t>(v.output), std::memory_order_relaxed);
    g_nrMotionResource[side].store(reinterpret_cast<uintptr_t>(v.motion), std::memory_order_relaxed);
    g_nrDepthResource[side].store(reinterpret_cast<uintptr_t>(v.depth), std::memory_order_relaxed);

    const uint64_t fp = NrParameterFingerprint(v);
    const uint64_t old = g_nrParameterFingerprint[side].exchange(fp, std::memory_order_relaxed);
    if (old != fp || sample == 1 || menu) {
        Log("[DLSSNR-DIAG][params] view=%s key=0x%llX menu=%d mask=0x%llX "
            "preset=%d style=%d intensity=%.3f tone=%.3f structure=%.3f skin=%.3f "
            "autoMask=%d ui=%d depthInv=%d reset=%d mv=%.3f/%.3f "
            "input=%ux%u output=%ux%u color=%p outputRes=%p mvRes=%p depth=%p\n",
            NrDiagSideName(side), key, menu ? 1 : 0,
            static_cast<unsigned long long>(v.mask), v.preset, v.style, v.intensity,
            v.localTone, v.localStructure, v.skinStructure, v.autoMask, v.uiCorrection,
            v.depthInverted, v.reset, v.motionX, v.motionY, v.inputWidth, v.inputHeight,
            v.outputWidth, v.outputHeight, v.color, v.output, v.motion, v.depth);
    }
}

uint32_t __fastcall HookedNrCreate(ID3D12GraphicsCommandList* list, uint32_t feature,
                                   void* params, void** outHandle) {
    const int side = NrDiagSide();
    const uint32_t result = g_nrCreateOrig(list, feature, params, outHandle);
    void* handle = nullptr;
    __try { if (outHandle) handle = *outHandle; }
    __except (EXCEPTION_EXECUTE_HANDLER) { handle = nullptr; }
    const uint64_t n = g_nrCreates[side].fetch_add(1, std::memory_order_relaxed) + 1;
    if (handle) g_nrLastHandle[side].store(reinterpret_cast<uintptr_t>(handle), std::memory_order_relaxed);
    if (handle && side < 2 &&
        g_nrLastHandle[1 - side].load(std::memory_order_relaxed) == reinterpret_cast<uintptr_t>(handle)) {
        g_nrSharedHandleEvals.fetch_add(1, std::memory_order_relaxed);
    }
    Log("[DLSSNR-DIAG][create] view=%s feature=%u result=0x%08X handle=%p params=%p list=%p count=%llu\n",
        NrDiagSideName(side), feature, result, handle, params, list,
        static_cast<unsigned long long>(n));
    return result;
}

uint32_t __fastcall HookedNrEvaluate(ID3D12GraphicsCommandList* list, const void* handle,
                                     const void* params, void* callback) {
    if (t_nrDiagInEvaluate) {
        g_nrRecursiveEvals.fetch_add(1, std::memory_order_relaxed);
        return g_nrEvaluateOrig(list, handle, params, callback);
    }
    struct Scope { Scope() { t_nrDiagInEvaluate = true; } ~Scope() { t_nrDiagInEvaluate = false; } } scope;

    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    const bool menuBefore = g_menuModeValue != 0;
    const uint64_t nextSideEval = g_nrEvals[side].load(std::memory_order_relaxed) + 1;
    const bool firstMenuSample = menuBefore &&
        g_nrMenuEvals[side].load(std::memory_order_relaxed) == 0;
    if (nextSideEval == 1 || (nextSideEval % 60) == 0 || firstMenuSample)
        SampleNrParameters(params, side, key, menuBefore);

    LARGE_INTEGER q0{}, q1{};
    QueryPerformanceCounter(&q0);
    const uint32_t result = g_nrEvaluateOrig(list, handle, params, callback);
    QueryPerformanceCounter(&q1);
    const uint64_t us = g_nrQpcFrequency
        ? static_cast<uint64_t>((q1.QuadPart - q0.QuadPart) * 1000000ull / g_nrQpcFrequency) : 0;

    const uint64_t sideN = g_nrEvals[side].fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t totalN = g_nrTotalEvals.fetch_add(1, std::memory_order_relaxed) + 1;
    g_nrTotalEvalUs.fetch_add(us, std::memory_order_relaxed);
    AtomicMax(g_nrMaxEvalUs, us);
    g_nrLastEvalTick[side].store(GetTickCount64(), std::memory_order_relaxed);
    g_nrLastHandle[side].store(reinterpret_cast<uintptr_t>(handle), std::memory_order_relaxed);
    g_nrLastParams[side].store(reinterpret_cast<uintptr_t>(params), std::memory_order_relaxed);
    g_nrLastResult[side].store(result, std::memory_order_relaxed);

    const bool menu = g_menuModeValue != 0;
    uint64_t menuN = 0;
    if (menu) menuN = g_nrMenuEvals[side].fetch_add(1, std::memory_order_relaxed) + 1;
    if (result & 0x80000000u) {
        const uint64_t bad = g_nrBadResults[side].fetch_add(1, std::memory_order_relaxed) + 1;
        if (bad <= 8 || (bad & (bad - 1)) == 0) {
            Log("[DLSSNR-DIAG][bad-result] view=%s key=0x%llX result=0x%08X handle=%p "
                "params=%p list=%p count=%llu\n", NrDiagSideName(side), key, result, handle,
                params, list, static_cast<unsigned long long>(bad));
        }
    }
    if (side < 2 && handle &&
        g_nrLastHandle[1 - side].load(std::memory_order_relaxed) == reinterpret_cast<uintptr_t>(handle)) {
        g_nrSharedHandleEvals.fetch_add(1, std::memory_order_relaxed);
    }

    if (sideN == 1 || menuN == 1) {
        Log("[DLSSNR-DIAG][eval-first] view=%s key=0x%llX menu=%d result=0x%08X "
            "handle=%p params=%p list=%p evalUs=%llu\n", NrDiagSideName(side), key,
            menu ? 1 : 0, result, handle, params, list, static_cast<unsigned long long>(us));
    }
    if ((totalN % 600) == 0) {
        const uint64_t totalUs = g_nrTotalEvalUs.load(std::memory_order_relaxed);
        Log("[DLSSNR-DIAG][eval] total=%llu MAIN=%llu VRCAM=%llu OTHER=%llu "
            "menu=%llu/%llu/%llu bad=%llu/%llu/%llu sharedHandle=%llu "
            "avgUs=%llu maxUs=%llu lastHandle=%p/%p\n",
            static_cast<unsigned long long>(totalN),
            static_cast<unsigned long long>(g_nrEvals[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrEvals[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrEvals[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrMenuEvals[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrMenuEvals[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrMenuEvals[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrBadResults[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrBadResults[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrBadResults[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSharedHandleEvals.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(totalN ? totalUs / totalN : 0),
            static_cast<unsigned long long>(g_nrMaxEvalUs.load(std::memory_order_relaxed)),
            reinterpret_cast<void*>(g_nrLastHandle[0].load(std::memory_order_relaxed)),
            reinterpret_cast<void*>(g_nrLastHandle[1].load(std::memory_order_relaxed)));
    }
    return result;
}

uint32_t __fastcall HookedNrRelease(const void* handle) {
    int side = 2;
    const uintptr_t h = reinterpret_cast<uintptr_t>(handle);
    for (int i = 0; i < 2; ++i) {
        uintptr_t expected = h;
        if (h && g_nrLastHandle[i].compare_exchange_strong(expected, 0, std::memory_order_relaxed)) {
            side = i;
            break;
        }
    }
    const uint32_t result = g_nrReleaseOrig(handle);
    const uint64_t n = g_nrReleases[side].fetch_add(1, std::memory_order_relaxed) + 1;
    if (n <= 16 || (n & (n - 1)) == 0) {
        Log("[DLSSNR-DIAG][release] view=%s result=0x%08X handle=%p count=%llu\n",
            NrDiagSideName(side), result, handle, static_cast<unsigned long long>(n));
    }
    return result;
}

bool PrologueEquals(const void* target, const uint8_t* expected, size_t count) {
    bool match = false;
    __try { match = target && memcmp(target, expected, count) == 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { match = false; }
    return match;
}

// The addon calls resource Set (measured slot 1) immediately before entering signed feature 18.
// Redirect only the parameter object's vptr to a cloned table: the implementation code is not
// patched, every call is forwarded unchanged, and all Get slots remain untouched. The first inner
// evaluation installs this recorder, so resource Set calls become visible on the next evaluation.
constexpr size_t kNrParamVtableSlots = 24;
void* g_nrParamOriginalVtable[kNrParamVtableSlots]{};
void* g_nrParamClonedVtable[kNrParamVtableSlots]{};
using NrSetResourceFn = void(__fastcall*)(void*, const char*, void*);
NrSetResourceFn g_nrSetResourceOriginal = nullptr;
std::mutex g_nrParamHookMutex;
std::atomic<bool> g_nrParamCloneReady{false};
std::atomic<uint64_t> g_nrSlot1Calls[3];
std::atomic<uint64_t> g_nrSlot1OuterCalls[3];
std::atomic<uint64_t> g_nrPostTakeoverInner[3];
std::atomic<uint64_t> g_nrPostTakeoverSlot[3];
std::atomic<bool> g_nrVrcamOuterSeen{false};
std::atomic<uint64_t> g_nrSlot1UnknownNames{0};

// Live foveation presets. The two centre boxes mirror UEVR's maximum-performance shape; the two
// full-height slabs preserve the successful Cyberpunk stereo geometry with only one temporal
// transition per eye. F10 writes a requested preset; the feature-18 prehook latches it only on a
// gameplay MAIN boundary so MAIN and the following VRCAM evaluation use identical geometry.
struct NrFovealPresetDef { int percent; bool nasalOpenSlab; const char* label; };
constexpr NrFovealPresetDef kNrFovealPresets[] = {
    {35, false, "35% Center Box (maximum performance)"},
    {50, false, "50% Center Box (performance)"},
    {65, true,  "65% Stereo Slab (balanced)"},
    {80, true,  "80% Stereo Slab (quality)"},
};
std::once_flag g_nrFovealConfigOnce;
char g_nrFovealConfigPath[MAX_PATH]{};
std::atomic<int> g_nrFovealActivePreset{2};
std::atomic<int> g_nrFovealSelectedPreset{2};
std::atomic<bool> g_nrFoveationEnabled{true};
std::atomic<ID3D12Resource*> g_nrFovealColor[3];
std::atomic<ID3D12Resource*> g_nrFovealOutput[3];
std::atomic<uint64_t> g_nrFovealApplies[3];
std::atomic<uint64_t> g_nrFovealCopies[3];
std::atomic<uint64_t> g_nrFovealRejects[3];

bool CopyNrParameterName(const char* src, char (&dst)[80]) {
    if (!src) return false;
    __try {
        size_t i = 0;
        for (; i + 1 < sizeof(dst); ++i) {
            const char c = src[i];
            dst[i] = c;
            if (c == '\0') return i != 0;
        }
        dst[sizeof(dst) - 1] = '\0';
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dst[0] = '\0';
        return false;
    }
}

void __fastcall RecordNrSetResource(void* self, const char* name, void* value) {
    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    const uint64_t n = g_nrSlot1Calls[side].fetch_add(1, std::memory_order_relaxed) + 1;
    unsigned long long outerSequence = 0;
    int outerSide = 2;
    const bool hasOuter = CyberpunkVR_GetSlEvaluateCurrentContext(&outerSequence, &outerSide) != 0;
    if (hasOuter && outerSide >= 0 && outerSide < 3)
        g_nrSlot1OuterCalls[outerSide].fetch_add(1, std::memory_order_relaxed);
    const int postSide = hasOuter && outerSide >= 0 && outerSide < 3 ? outerSide : side;
    if (g_nrVrcamOuterSeen.load(std::memory_order_relaxed))
        g_nrPostTakeoverSlot[postSide].fetch_add(1, std::memory_order_relaxed);
    char safeName[80]{};
    const bool named = CopyNrParameterName(name, safeName);
    if (!named) {
        g_nrSlot1UnknownNames.fetch_add(1, std::memory_order_relaxed);
    } else if (side >= 0 && side < 3) {
        if (_stricmp(safeName, "DLSSNR.Color") == 0)
            g_nrFovealColor[side].store(static_cast<ID3D12Resource*>(value), std::memory_order_release);
        else if (_stricmp(safeName, "DLSSNR.Output") == 0)
            g_nrFovealOutput[side].store(static_cast<ID3D12Resource*>(value), std::memory_order_release);
    }
    if (n <= 16 || (n % 601) == 0) {
        Log("[DLSSNR-DIAG][slot1] view=%s key=0x%llX count=%llu name=%s value=%p "
            "outer=%d outerSeq=%llu outerSide=%s\n",
            NrDiagSideName(side), static_cast<unsigned long long>(key),
            static_cast<unsigned long long>(n), named ? safeName : "<unreadable>", value,
            hasOuter ? 1 : 0, outerSequence, NrDiagSideName(outerSide));
    }
    // Exact transparent forwarding: no pointer substitution in this phase.
    g_nrSetResourceOriginal(self, name, value);
}

int ReadNrParameterVptr(const void* params, void*** currentOut) {
    __try {
        void** current = *reinterpret_cast<void***>(const_cast<void*>(params));
        if (!current) return 1;
        *currentOut = current;
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 2; }
}

int CopyNrParameterVtable(void** current) {
    __try {
        memcpy(g_nrParamOriginalVtable, current, sizeof(g_nrParamOriginalVtable));
        memcpy(g_nrParamClonedVtable, current, sizeof(g_nrParamClonedVtable));
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}

bool WriteNrParameterVptr(const void* params, void** replacement) {
    __try {
        *reinterpret_cast<void***>(const_cast<void*>(params)) = replacement;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void InstallNrSlot1Recorder(const void* params) {
    if (!params) return;
    std::lock_guard<std::mutex> lock(g_nrParamHookMutex);
    void** current = nullptr;
    const int read = ReadNrParameterVptr(params, &current);
    if (read != 0) {
        Log("[DLSSNR-DIAG][slot1] could not read parameter vtable params=%p code=%d; refused\n",
            params, read);
        return;
    }
    if (current == g_nrParamClonedVtable) return;
    if (!g_nrParamCloneReady.load(std::memory_order_acquire)) {
        if (CopyNrParameterVtable(current) != 0) {
            Log("[DLSSNR-DIAG][slot1] exception while cloning parameter vtable params=%p; refused\n",
                params);
            return;
        }
        g_nrSetResourceOriginal = reinterpret_cast<NrSetResourceFn>(current[1]);
        if (!g_nrSetResourceOriginal) {
            Log("[DLSSNR-DIAG][slot1] measured resource setter slot is null; refusing recorder\n");
            return;
        }
        g_nrParamClonedVtable[1] = reinterpret_cast<void*>(&RecordNrSetResource);
        g_nrParamCloneReady.store(true, std::memory_order_release);
    } else if (current[1] != reinterpret_cast<void*>(g_nrSetResourceOriginal)) {
        Log("[DLSSNR-DIAG][slot1] parameter vtable differs at measured slot 1; refusing "
            "params=%p current=%p expected=%p\n", params, current[1],
            reinterpret_cast<void*>(g_nrSetResourceOriginal));
        return;
    }
    if (!WriteNrParameterVptr(params, g_nrParamClonedVtable)) {
        Log("[DLSSNR-DIAG][slot1] exception replacing parameter vptr params=%p; refused\n", params);
        return;
    }
    Log("[DLSSNR-DIAG][slot1] transparent resource recorder armed params=%p original=%p\n",
        params, reinterpret_cast<void*>(g_nrSetResourceOriginal));
}

struct NrSubrect {
    uint32_t x = 0, y = 0, w = 0, h = 0;
    bool valid = false;
};

void NrSubrectName(char (&out)[64], const char* plane, const char* field) {
    _snprintf_s(out, sizeof(out), _TRUNCATE, "DLSSNR.%sSubrect%s", plane, field);
}

NrSubrect ReadNrSubrect(const void* params, const char* plane) {
    NrSubrect r{};
    char name[64]{};
    NrSubrectName(name, plane, "BaseX");  const bool a = NrParamGetUInt(params, name, &r.x);
    NrSubrectName(name, plane, "BaseY");  const bool b = NrParamGetUInt(params, name, &r.y);
    NrSubrectName(name, plane, "Width");  const bool c = NrParamGetUInt(params, name, &r.w);
    NrSubrectName(name, plane, "Height"); const bool d = NrParamGetUInt(params, name, &r.h);
    r.valid = a && b && c && d && r.w >= 64 && r.h >= 64 && r.w <= 16384 && r.h <= 16384;
    return r;
}

bool SetNrUInt(void* params, const char* name, uint32_t value) {
    if (!params || !name) return false;
    __try {
        void** vt = *reinterpret_cast<void***>(params);
        using Fn = void(__fastcall*)(void*, const char*, uint32_t);
        reinterpret_cast<Fn>(vt[3])(params, name, value); // measured/documented unsigned Set
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

uint32_t NrAlign8Down(uint32_t value) { return value & ~7u; }

void LoadNrFovealConfig() {
    std::call_once(g_nrFovealConfigOnce, [] {
        strncpy_s(g_nrFovealConfigPath, g_liveControlPath, _TRUNCATE);
        char* slash = strrchr(g_nrFovealConfigPath, '\\');
        if (!slash) slash = strrchr(g_nrFovealConfigPath, '/');
        if (slash) strcpy_s(slash + 1, MAX_PATH - static_cast<size_t>(slash + 1 - g_nrFovealConfigPath),
                            "nr-foveated.ini");
        else strcpy_s(g_nrFovealConfigPath, "nr-foveated.ini");
        int preset = GetPrivateProfileIntA("DLSSNRFoveation", "Preset", 2, g_nrFovealConfigPath);
        if (preset < 0 || preset >= static_cast<int>(std::size(kNrFovealPresets))) preset = 2;
        g_nrFovealActivePreset.store(preset, std::memory_order_relaxed);
        g_nrFovealSelectedPreset.store(preset, std::memory_order_relaxed);
        if (GetFileAttributesA(g_nrFovealConfigPath) == INVALID_FILE_ATTRIBUTES) {
            char value[8]{}; _snprintf_s(value, sizeof(value), _TRUNCATE, "%d", preset);
            WritePrivateProfileStringA("DLSSNRFoveation", "Preset", value, g_nrFovealConfigPath);
        }
        Log("[DLSSNR-FOV] initial preset %d: %s (live pair-latched, config=%s)\n", preset,
            kNrFovealPresets[preset].label, g_nrFovealConfigPath);
    });
}

const NrFovealPresetDef& ActiveNrFovealPreset() {
    LoadNrFovealConfig();
    int p = g_nrFovealActivePreset.load(std::memory_order_relaxed);
    if (p < 0 || p >= static_cast<int>(std::size(kNrFovealPresets))) p = 2;
    return kNrFovealPresets[p];
}

void LatchNrFovealPresetAtPairBoundary(int side, bool menu, bool hasOuter, int outerSide) {
    // MAIN is always the first physical eye in the proven steady sequence. Never latch from the UI
    // thread or from VRCAM: one atomic update here keeps the complete MAIN->VRCAM pair coherent.
    if (menu || side != 0 || !hasOuter || outerSide != 0 ||
        !g_nrVrcamOuterSeen.load(std::memory_order_relaxed)) return;
    LoadNrFovealConfig();
    const int selected = g_nrFovealSelectedPreset.load(std::memory_order_acquire);
    const int active = g_nrFovealActivePreset.load(std::memory_order_relaxed);
    if (selected == active || selected < 0 ||
        selected >= static_cast<int>(std::size(kNrFovealPresets))) return;
    g_nrFovealActivePreset.store(selected, std::memory_order_release);
    Log("[DLSSNR-FOV] live preset latched at MAIN boundary: %d (%s), previous=%d (%s)\n",
        selected, kNrFovealPresets[selected].label, active, kNrFovealPresets[active].label);
}

bool ComputeNrRegion(const NrSubrect& r, int side, const NrFovealPresetDef& preset,
                     uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) {
    if (!r.valid || (side != 0 && side != 1) || !x || !y || !w || !h) return false;
    const float fraction = static_cast<float>(preset.percent) * 0.01f;
    *w = NrAlign8Down(static_cast<uint32_t>(r.w * fraction));
    *h = preset.nasalOpenSlab ? r.h : NrAlign8Down(static_cast<uint32_t>(r.h * fraction));
    if (*w < 64 || *h < 64 || *w >= r.w || *h > r.h) return false;
    if (preset.nasalOpenSlab) {
        const uint32_t crop = r.w - *w;
        *x = side == 1 ? r.x + crop : r.x; // VRCAM/left opens right; MAIN/right opens left
        *y = r.y;
    } else {
        // Match the UEVR/OpenXR-Toolkit starting point: left eye +4%, right eye -4% so the two
        // centre boxes overlap the same world region better than identical image coordinates.
        const int shift = static_cast<int>(r.w * 0.04f) * (side == 1 ? 1 : -1);
        int cx = static_cast<int>(r.x + (r.w - *w) / 2) + shift;
        cx = std::max<int>(static_cast<int>(r.x),
                           std::min<int>(cx, static_cast<int>(r.x + r.w - *w)));
        *x = NrAlign8Down(static_cast<uint32_t>(cx));
        *y = r.y + NrAlign8Down((r.h - *h) / 2);
    }
    return *x >= r.x && *y >= r.y && *x + *w <= r.x + r.w && *y + *h <= r.y + r.h;
}

bool ApplyNrFovealRegion(void* params, int side, const NrFovealPresetDef& preset,
                         NrSubrect* originalOutput, uint32_t* activeX, uint32_t* activeY,
                         uint32_t* activeW, uint32_t* activeH) {
    if (!params || (side != 0 && side != 1)) return false;
    static constexpr const char* planes[] = {"Color", "Depth", "MVec", "Output"};
    NrSubrect rects[4]{};
    uint32_t xs[4]{}, ys[4]{}, widths[4]{}, heights[4]{};
    for (int i = 0; i < 4; ++i) {
        rects[i] = ReadNrSubrect(params, planes[i]);
        if (!ComputeNrRegion(rects[i], side, preset, &xs[i], &ys[i], &widths[i], &heights[i])) return false;
    }
    for (int i = 0; i < 4; ++i) {
        char name[64]{};
        NrSubrectName(name, planes[i], "BaseX");  if (!SetNrUInt(params, name, xs[i])) return false;
        NrSubrectName(name, planes[i], "BaseY");  if (!SetNrUInt(params, name, ys[i])) return false;
        NrSubrectName(name, planes[i], "Width");  if (!SetNrUInt(params, name, widths[i])) return false;
        NrSubrectName(name, planes[i], "Height"); if (!SetNrUInt(params, name, heights[i])) return false;
    }
    if (originalOutput) *originalOutput = rects[3];
    if (activeX) *activeX = xs[3]; if (activeY) *activeY = ys[3];
    if (activeW) *activeW = widths[3]; if (activeH) *activeH = heights[3];
    return true;
}

// Seed only bands outside the active region. Whole-resource copies are prohibited because the
// sequential second eye could overwrite the first eye's neural result in the singleton Output.
int RefreshNrPeripheryBands(ID3D12GraphicsCommandList* list, ID3D12Resource* color,
                            ID3D12Resource* output, const NrSubrect& full,
                            uint32_t ax, uint32_t ay, uint32_t aw, uint32_t ah) {
    if (!list || !color || !output || color == output) return 1;
    __try {
        const D3D12_RESOURCE_DESC c = color->GetDesc();
        const D3D12_RESOURCE_DESC o = output->GetDesc();
        if (c.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            o.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            c.Width != o.Width || c.Height != o.Height || c.Format != o.Format ||
            full.x != 0 || full.y != 0 || full.w != o.Width || full.h != o.Height ||
            ax < full.x || ay < full.y || ax + aw > full.w || ay + ah > full.h)
            return 2;

        D3D12_RESOURCE_BARRIER toCopy[2]{};
        toCopy[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy[0].Transition.pResource = color;
        toCopy[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toCopy[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toCopy[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toCopy[1] = toCopy[0]; toCopy[1].Transition.pResource = output;
        toCopy[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        D3D12_RESOURCE_BARRIER back[2] = {toCopy[0], toCopy[1]};
        back[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        back[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        back[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        back[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
        dst.pResource = output; dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.pResource = color; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        struct Band { uint32_t l, t, r, b; };
        const Band bands[4] = {
            {full.x, full.y, full.x + full.w, ay},
            {full.x, ay + ah, full.x + full.w, full.y + full.h},
            {full.x, ay, ax, ay + ah},
            {ax + aw, ay, full.x + full.w, ay + ah},
        };
        list->ResourceBarrier(2, toCopy);
        for (const Band& band : bands) {
            if (band.r <= band.l || band.b <= band.t) continue;
            D3D12_BOX box{band.l, band.t, 0, band.r, band.b, 1};
            list->CopyTextureRegion(&dst, band.l, band.t, 0, &src, &box);
        }
        list->ResourceBarrier(2, back);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 3; }
}

// MinHook's generated trampoline is valid, but CALLing it changes the return address observed by
// signed DLSSNR. This prehook is reached by CALL from our private thunk; the thunk then restores all
// four original argument registers and JMPs to MinHook's trampoline. The runtime therefore sees
// the addon's untouched return address. There is deliberately no posthook and no result mutation.
void __fastcall NrTailPrehook(ID3D12GraphicsCommandList* list, const void* handle,
                              const void* params) {
    if (t_nrDiagInEvaluate) {
        g_nrRecursiveEvals.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    struct Scope { Scope() { t_nrDiagInEvaluate = true; } ~Scope() { t_nrDiagInEvaluate = false; } } scope;

    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    const uint64_t sideN = g_nrEvals[side].fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t totalN = g_nrTotalEvals.fetch_add(1, std::memory_order_relaxed) + 1;
    g_nrLastEvalTick[side].store(GetTickCount64(), std::memory_order_relaxed);
    g_nrLastHandle[side].store(reinterpret_cast<uintptr_t>(handle), std::memory_order_relaxed);
    g_nrLastParams[side].store(reinterpret_cast<uintptr_t>(params), std::memory_order_relaxed);
    g_nrLastResult[side].store(0xFFFFFFFFu, std::memory_order_relaxed); // tail-jump has no return path

    unsigned long long outerSequence = 0;
    int outerSide = 2;
    const bool hasOuter = CyberpunkVR_GetSlEvaluateCurrentContext(&outerSequence, &outerSide) != 0;
    if (g_nrVrcamOuterSeen.load(std::memory_order_relaxed))
        g_nrPostTakeoverInner[side].fetch_add(1, std::memory_order_relaxed);
    const bool menu = g_menuModeValue != 0;
    if (menu) g_nrMenuEvals[side].fetch_add(1, std::memory_order_relaxed);
    LatchNrFovealPresetAtPairBoundary(side, menu, hasOuter, outerSide);
    if (sideN == 1 || (sideN % 60) == 0)
        SampleNrParameters(params, side, key, menu);
    InstallNrSlot1Recorder(params);

    if (g_nrFoveationEnabled.load(std::memory_order_relaxed) && !menu &&
        g_nrVrcamOuterSeen.load(std::memory_order_relaxed) && hasOuter &&
        (side == 0 || side == 1) && outerSide == side) {
        ID3D12Resource* color = g_nrFovealColor[side].load(std::memory_order_acquire);
        ID3D12Resource* output = g_nrFovealOutput[side].load(std::memory_order_acquire);
        const NrSubrect full = ReadNrSubrect(params, "Output");
        const NrFovealPresetDef preset = ActiveNrFovealPreset();
        uint32_t x = 0, y = 0, width = 0, height = 0;
        const bool region = ComputeNrRegion(full, side, preset, &x, &y, &width, &height);
        const int copied = region
            ? RefreshNrPeripheryBands(list, color, output, full, x, y, width, height) : 2;
        NrSubrect appliedFull{}; uint32_t appliedX = 0, appliedY = 0, appliedW = 0, appliedH = 0;
        const bool applied = copied == 0 && ApplyNrFovealRegion(
            const_cast<void*>(params), side, preset, &appliedFull,
            &appliedX, &appliedY, &appliedW, &appliedH);
        if (applied) {
            const uint64_t n = g_nrFovealApplies[side].fetch_add(1, std::memory_order_relaxed) + 1;
            g_nrFovealCopies[side].fetch_add(1, std::memory_order_relaxed);
            if (n <= 4 || (n % 601) == 0) {
                const NrSubrect back = ReadNrSubrect(params, "Output");
                Log("[DLSSNR-FOV] view=%s apply=%llu preset=%s full=(%u,%u %ux%u) "
                    "active=(%u,%u %ux%u) copy=bands readback=%s\n",
                    NrDiagSideName(side), static_cast<unsigned long long>(n), preset.label,
                    appliedFull.x, appliedFull.y, appliedFull.w, appliedFull.h,
                    appliedX, appliedY, appliedW, appliedH,
                    back.valid && back.x == appliedX && back.w == appliedW &&
                    back.y == appliedY && back.h == appliedH ? "OK" : "MISMATCH");
            }
        } else {
            const uint64_t n = g_nrFovealRejects[side].fetch_add(1, std::memory_order_relaxed) + 1;
            if (n <= 8 || (n % 601) == 0) {
                Log("[DLSSNR-FOV] view=%s rejected=%llu resources=%p/%p fullValid=%d copy=%d\n",
                    NrDiagSideName(side), static_cast<unsigned long long>(n), color, output,
                    full.valid ? 1 : 0, copied);
            }
        }
    }

    if (sideN <= 8 || (sideN % 601) == 0) {
        Log("[DLSSNR-DIAG][tail-eval] total=%llu view=%s key=0x%llX sideCount=%llu "
            "handle=%p params=%p list=%p outer=%d outerSeq=%llu outerSide=%s menu=%d\n",
            static_cast<unsigned long long>(totalN), NrDiagSideName(side),
            static_cast<unsigned long long>(key), static_cast<unsigned long long>(sideN), handle,
            params, list, hasOuter ? 1 : 0, outerSequence, NrDiagSideName(outerSide), menu ? 1 : 0);
    }
    if ((totalN % 600) == 0) {
        Log("[DLSSNR-DIAG][tail-summary] inner=%llu/%llu/%llu slotView=%llu/%llu/%llu "
            "slotOuter=%llu/%llu/%llu postVrcamInner=%llu/%llu/%llu "
            "postVrcamSlot=%llu/%llu/%llu unknownNames=%llu\n",
            static_cast<unsigned long long>(g_nrEvals[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrEvals[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrEvals[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSlot1Calls[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSlot1Calls[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSlot1Calls[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSlot1OuterCalls[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSlot1OuterCalls[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSlot1OuterCalls[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrPostTakeoverInner[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrPostTakeoverInner[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrPostTakeoverInner[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrPostTakeoverSlot[0].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrPostTakeoverSlot[1].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrPostTakeoverSlot[2].load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_nrSlot1UnknownNames.load(std::memory_order_relaxed)));
    }
}

// Same register-preserving shape proven in two UEVR-hosted games, except this prehook consumes all
// first three original arguments directly (rcx=list, rdx=handle, r8=params).
constexpr size_t kNrTailStubSize = 0x48;
constexpr size_t kNrTailPrehookImmediate = 0x1A;
constexpr size_t kNrTailTargetImmediate = 0x3E;
constexpr uint8_t kNrTailStubCode[kNrTailStubSize] = {
    0x48,0x83,0xEC,0x48,
    0x48,0x89,0x4C,0x24,0x20,
    0x48,0x89,0x54,0x24,0x28,
    0x4C,0x89,0x44,0x24,0x30,
    0x4C,0x89,0x4C,0x24,0x38,
    0x48,0xB8,0,0,0,0,0,0,0,0,
    0xFF,0xD0,
    0x48,0x8B,0x4C,0x24,0x20,
    0x48,0x8B,0x54,0x24,0x28,
    0x4C,0x8B,0x44,0x24,0x30,
    0x4C,0x8B,0x4C,0x24,0x38,
    0x48,0x83,0xC4,0x48,
    0x48,0xB8,0,0,0,0,0,0,0,0,
    0xFF,0xE0,
};
uint8_t* g_nrTailStub = nullptr;
void* g_nrTailTarget = nullptr;

uint8_t* BuildNrTailStub() {
    auto* stub = static_cast<uint8_t*>(VirtualAlloc(
        nullptr, kNrTailStubSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!stub) return nullptr;
    memcpy(stub, kNrTailStubCode, kNrTailStubSize);
    const uint64_t prehook = reinterpret_cast<uint64_t>(&NrTailPrehook);
    memcpy(stub + kNrTailPrehookImmediate, &prehook, sizeof(prehook));
    return stub;
}

// RenoDX 4.55 outer ownership census. Static analysis of the exact deployed addon identified
// +0x303F0 as the handle-eligibility predicate called after each original DLSS evaluation and
// immediately before +0x30A50 enters the inline NR path. These are ordinary addon functions, not
// signed DLSSNR exports, so transparent MinHook CALL forwarding preserves the signed runtime's
// caller (the addon itself). Create/release wrappers maintain an independent read-only handle ledger
// to distinguish a game release from the addon's registry forgetting a still-live feature.
using RenoEligibleFn = bool(__fastcall*)(const void*);
using RenoCreateFn = uint32_t(__fastcall*)(void*, uint32_t, void*, void**);
using RenoReleaseFn = uint32_t(__fastcall*)(const void*);
using RenoCaptureStateFn = void(__fastcall*)(void*, void*);
RenoEligibleFn g_renoEligibleOrig = nullptr;
RenoCreateFn g_renoCreateOrig[2]{};
RenoReleaseFn g_renoReleaseOrig[2]{};
RenoCaptureStateFn g_renoCaptureStateOrig = nullptr;
std::atomic<int> g_renoOuterState{0};
std::atomic<int> g_renoStereoBypassState{0};
std::atomic<uint64_t> g_renoDuplicateBypass[3];
std::atomic<uint64_t> g_renoDuplicateKept[3];
uint8_t* g_renoDuplicateStub = nullptr;
uint8_t* AllocateExecNearby(void* anchor, size_t bytes);
std::atomic<uint64_t> g_renoEligiblePass[3];
std::atomic<uint64_t> g_renoEligibleFail[3];
std::atomic<int> g_renoEligibleLast[3]{{-1}, {-1}, {-1}};
std::atomic<uint64_t> g_renoCaptureSamples[3];
std::atomic<int> g_renoCaptureLastMask[3]{{-1}, {-1}, {-1}};

struct RenoHandleLedgerEntry {
    std::atomic<uintptr_t> handle{0};
    std::atomic<uint32_t> feature{0xFFFFFFFFu};
    std::atomic<uint32_t> createSlot{0xFFFFFFFFu};
    std::atomic<uint32_t> releases{0};
};
RenoHandleLedgerEntry g_renoHandles[32];

const void* ReadRenoCreatedHandle(void** outHandle) {
    __try { return outHandle ? *outHandle : nullptr; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

RenoHandleLedgerEntry* FindRenoHandle(const void* handle) {
    const uintptr_t key = reinterpret_cast<uintptr_t>(handle);
    if (!key) return nullptr;
    for (auto& entry : g_renoHandles) {
        if (entry.handle.load(std::memory_order_acquire) == key) return &entry;
    }
    return nullptr;
}

void RecordRenoCreate(const void* handle, uint32_t feature, uint32_t slot, uint32_t result) {
    if (!handle) return;
    RenoHandleLedgerEntry* entry = FindRenoHandle(handle);
    if (!entry) {
        const uintptr_t key = reinterpret_cast<uintptr_t>(handle);
        for (auto& candidate : g_renoHandles) {
            uintptr_t empty = 0;
            if (candidate.handle.compare_exchange_strong(empty, key, std::memory_order_acq_rel)) {
                entry = &candidate;
                break;
            }
        }
    }
    if (entry) {
        entry->feature.store(feature, std::memory_order_release);
        entry->createSlot.store(slot, std::memory_order_release);
        entry->releases.store(0, std::memory_order_release);
    }
    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    unsigned long long outerSequence = 0;
    int outerSide = 2;
    const bool hasOuter = CyberpunkVR_GetSlEvaluateCurrentContext(&outerSequence, &outerSide) != 0;
    Log("[DLSSNR-DIAG][outer-create] slot=%u feature=%u handle=%p result=0x%08X "
        "view=%s key=0x%llX outer=%d outerSeq=%llu outerSide=%s ledger=%d\n",
        slot, feature, handle, result, NrDiagSideName(side),
        static_cast<unsigned long long>(key), hasOuter ? 1 : 0, outerSequence,
        NrDiagSideName(outerSide), entry ? 1 : 0);
}

uint32_t __fastcall HookedRenoCreate0(void* context, uint32_t feature, void* params,
                                      void** outHandle) {
    const uint32_t result = g_renoCreateOrig[0](context, feature, params, outHandle);
    RecordRenoCreate(ReadRenoCreatedHandle(outHandle), feature, 0, result);
    return result;
}

uint32_t __fastcall HookedRenoCreate1(void* context, uint32_t feature, void* params,
                                      void** outHandle) {
    const uint32_t result = g_renoCreateOrig[1](context, feature, params, outHandle);
    RecordRenoCreate(ReadRenoCreatedHandle(outHandle), feature, 1, result);
    return result;
}

void RecordRenoRelease(const void* handle, uint32_t slot, uint32_t result) {
    RenoHandleLedgerEntry* entry = FindRenoHandle(handle);
    const uint32_t feature = entry
        ? entry->feature.load(std::memory_order_acquire) : 0xFFFFFFFFu;
    const uint32_t releases = entry
        ? entry->releases.fetch_add(1, std::memory_order_acq_rel) + 1 : 0;
    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    unsigned long long outerSequence = 0;
    int outerSide = 2;
    const bool hasOuter = CyberpunkVR_GetSlEvaluateCurrentContext(&outerSequence, &outerSide) != 0;
    Log("[DLSSNR-DIAG][outer-release] slot=%u handle=%p feature=%u releases=%u "
        "result=0x%08X view=%s key=0x%llX outer=%d outerSeq=%llu outerSide=%s\n",
        slot, handle, feature, releases, result, NrDiagSideName(side),
        static_cast<unsigned long long>(key), hasOuter ? 1 : 0, outerSequence,
        NrDiagSideName(outerSide));
}

uint32_t __fastcall HookedRenoRelease0(const void* handle) {
    const uint32_t result = g_renoReleaseOrig[0](handle);
    RecordRenoRelease(handle, 0, result);
    return result;
}

uint32_t __fastcall HookedRenoRelease1(const void* handle) {
    const uint32_t result = g_renoReleaseOrig[1](handle);
    RecordRenoRelease(handle, 1, result);
    return result;
}

bool __fastcall HookedRenoEligible(const void* handle) {
    const bool eligible = g_renoEligibleOrig(handle);
    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    unsigned long long outerSequence = 0;
    int outerSide = 2;
    const bool hasOuter = CyberpunkVR_GetSlEvaluateCurrentContext(&outerSequence, &outerSide) != 0;
    RenoHandleLedgerEntry* entry = FindRenoHandle(handle);
    const uint32_t feature = entry
        ? entry->feature.load(std::memory_order_acquire) : 0xFFFFFFFFu;
    const uint32_t createSlot = entry
        ? entry->createSlot.load(std::memory_order_acquire) : 0xFFFFFFFFu;
    const uint32_t releases = entry
        ? entry->releases.load(std::memory_order_acquire) : 0;
    const uint64_t count = (eligible ? g_renoEligiblePass[side] : g_renoEligibleFail[side])
        .fetch_add(1, std::memory_order_relaxed) + 1;
    const int prior = g_renoEligibleLast[side].exchange(eligible ? 1 : 0,
                                                        std::memory_order_acq_rel);
    if (count <= 8 || prior != (eligible ? 1 : 0) || (count % 600) == 0) {
        Log("[DLSSNR-DIAG][outer-eligible] view=%s key=0x%llX handle=%p eligible=%d "
            "count=%llu transition=%d ledger=%d feature=%u createSlot=%u releases=%u "
            "outer=%d outerSeq=%llu outerSide=%s\n",
            NrDiagSideName(side), static_cast<unsigned long long>(key), handle,
            eligible ? 1 : 0, static_cast<unsigned long long>(count),
            prior == -1 || prior != (eligible ? 1 : 0) ? 1 : 0, entry ? 1 : 0,
            feature, createSlot, releases, hasOuter ? 1 : 0, outerSequence,
            NrDiagSideName(outerSide));
    }
    return eligible;
}

struct RenoCaptureSnapshot {
    uint8_t mask;
    uintptr_t values[5];
};

bool ReadRenoCaptureSnapshot(const void* snapshot, RenoCaptureSnapshot* out) {
    if (!snapshot || !out) return false;
    __try {
        const auto* bytes = reinterpret_cast<const uint8_t*>(snapshot);
        uint8_t mask = bytes[0] == 1 ? 0x01 : 0;
        if (bytes[1]) mask |= 0x02;
        if (bytes[0x10]) mask |= 0x04;
        if (bytes[0x20]) mask |= 0x08;
        if (bytes[0x30]) mask |= 0x10;
        if (bytes[0x40]) mask |= 0x20;
        out->mask = mask;
        memcpy(&out->values[0], bytes + 0x08, sizeof(uintptr_t));
        memcpy(&out->values[1], bytes + 0x18, sizeof(uintptr_t));
        memcpy(&out->values[2], bytes + 0x28, sizeof(uintptr_t));
        memcpy(&out->values[3], bytes + 0x38, sizeof(uintptr_t));
        memcpy(&out->values[4], bytes + 0x48, sizeof(uintptr_t));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void __fastcall HookedRenoCaptureState(void* snapshot, void* trackedState) {
    g_renoCaptureStateOrig(snapshot, trackedState);
    RenoCaptureSnapshot captured{};
    const bool readable = ReadRenoCaptureSnapshot(snapshot, &captured);
    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    unsigned long long outerSequence = 0;
    int outerSide = 2;
    const bool hasOuter = CyberpunkVR_GetSlEvaluateCurrentContext(&outerSequence, &outerSide) != 0;
    const uint64_t count = g_renoCaptureSamples[side].fetch_add(1, std::memory_order_relaxed) + 1;
    const int mask = readable ? static_cast<int>(captured.mask) : -2;
    const int prior = g_renoCaptureLastMask[side].exchange(mask, std::memory_order_acq_rel);
    if (count <= 8 || prior != mask || (count % 600) == 0) {
        Log("[DLSSNR-DIAG][outer-state] view=%s key=0x%llX count=%llu state=%p "
            "readable=%d mask=0x%02X complete=%d transition=%d "
            "v=%p/%p/%p/%p/%p outer=%d outerSeq=%llu outerSide=%s\n",
            NrDiagSideName(side), static_cast<unsigned long long>(key),
            static_cast<unsigned long long>(count), trackedState, readable ? 1 : 0,
            readable ? captured.mask : 0, readable && captured.mask == 0x3F ? 1 : 0,
            prior != mask ? 1 : 0, reinterpret_cast<void*>(captured.values[0]),
            reinterpret_cast<void*>(captured.values[1]), reinterpret_cast<void*>(captured.values[2]),
            reinterpret_cast<void*>(captured.values[3]), reinterpret_cast<void*>(captured.values[4]),
            hasOuter ? 1 : 0, outerSequence, NrDiagSideName(outerSide));
    }
}

int __fastcall ShouldBypassRenoDuplicate() {
    uint64_t key = 0;
    const int side = NrDiagSide(&key);
    unsigned long long outerSequence = 0;
    int outerSide = 2;
    const bool hasOuter = CyberpunkVR_GetSlEvaluateCurrentContext(&outerSequence, &outerSide) != 0;
    const bool bypass = g_nrVrcamOuterSeen.load(std::memory_order_relaxed)
        && hasOuter && (side == 0 || side == 1) && outerSide == side;
    const uint64_t count = (bypass ? g_renoDuplicateBypass[side] : g_renoDuplicateKept[side])
        .fetch_add(1, std::memory_order_relaxed) + 1;
    if (count <= 8 || (count % 600) == 0) {
        Log("[DLSSNR-DIAG][dedup-branch] action=%s view=%s key=0x%llX count=%llu "
            "takeover=%d outer=%d outerSeq=%llu outerSide=%s\n",
            bypass ? "BYPASS" : "KEEP", NrDiagSideName(side),
            static_cast<unsigned long long>(key), static_cast<unsigned long long>(count),
            g_nrVrcamOuterSeen.load(std::memory_order_relaxed) ? 1 : 0,
            hasOuter ? 1 : 0, outerSequence, NrDiagSideName(outerSide));
    }
    return bypass ? 1 : 0;
}

void EmitRenoStub8(uint8_t* stub, size_t* pos, uint8_t value) {
    stub[(*pos)++] = value;
}

void EmitRenoStub32(uint8_t* stub, size_t* pos, uint32_t value) {
    memcpy(stub + *pos, &value, sizeof(value));
    *pos += sizeof(value);
}

void EmitRenoStub64(uint8_t* stub, size_t* pos, uint64_t value) {
    memcpy(stub + *pos, &value, sizeof(value));
    *pos += sizeof(value);
}

void EmitRenoXmmStack(uint8_t* stub, size_t* pos, bool load, uint8_t xmm, uint32_t offset) {
    EmitRenoStub8(stub, pos, 0xF3);
    EmitRenoStub8(stub, pos, 0x0F);
    EmitRenoStub8(stub, pos, load ? 0x6F : 0x7F); // MOVDQU xmm,[rsp+n] / [rsp+n],xmm
    if (offset < 0x80) {
        EmitRenoStub8(stub, pos, static_cast<uint8_t>(0x44 | (xmm << 3)));
        EmitRenoStub8(stub, pos, 0x24);
        EmitRenoStub8(stub, pos, static_cast<uint8_t>(offset));
    } else {
        EmitRenoStub8(stub, pos, static_cast<uint8_t>(0x84 | (xmm << 3)));
        EmitRenoStub8(stub, pos, 0x24);
        EmitRenoStub32(stub, pos, offset);
    }
}

uint8_t* BuildRenoDuplicateBranchStub(uint8_t* branch, uint8_t* normalTarget,
                                      uint8_t* skipTarget, size_t* outSize) {
    constexpr size_t kCapacity = 256;
    uint8_t* stub = AllocateExecNearby(branch, kCapacity);
    if (!stub) return nullptr;
    size_t p = 0;

    // Preserve the original compare's ZF. Non-equal calls jump directly to normal fallthrough.
    EmitRenoStub8(stub, &p, 0x0F); EmitRenoStub8(stub, &p, 0x85);
    const size_t originalJneDisp = p; EmitRenoStub32(stub, &p, 0);

    // Equal path: reserve shadow/save space while retaining ABI alignment, then preserve every
    // volatile GPR and XMM register across the C decision callback.
    const uint8_t subRsp[] = {0x48,0x81,0xEC,0xD0,0x00,0x00,0x00};
    memcpy(stub + p, subRsp, sizeof(subRsp)); p += sizeof(subRsp);
    const uint8_t saveGprs[] = {
        0x48,0x89,0x44,0x24,0x20, 0x48,0x89,0x4C,0x24,0x28,
        0x48,0x89,0x54,0x24,0x30, 0x4C,0x89,0x44,0x24,0x38,
        0x4C,0x89,0x4C,0x24,0x40, 0x4C,0x89,0x54,0x24,0x48,
        0x4C,0x89,0x5C,0x24,0x50,
    };
    memcpy(stub + p, saveGprs, sizeof(saveGprs)); p += sizeof(saveGprs);
    for (uint8_t i = 0; i < 6; ++i) EmitRenoXmmStack(stub, &p, false, i, 0x60 + i * 0x10);

    EmitRenoStub8(stub, &p, 0x48); EmitRenoStub8(stub, &p, 0xB8);
    EmitRenoStub64(stub, &p, reinterpret_cast<uint64_t>(&ShouldBypassRenoDuplicate));
    EmitRenoStub8(stub, &p, 0xFF); EmitRenoStub8(stub, &p, 0xD0); // call rax
    EmitRenoStub8(stub, &p, 0x85); EmitRenoStub8(stub, &p, 0xC0); // test eax,eax

    // MOVDQU and MOV do not alter flags, and LEA restores RSP without altering flags, so the JZ
    // below still consumes the callback decision after all original volatile state is restored.
    for (int i = 5; i >= 0; --i)
        EmitRenoXmmStack(stub, &p, true, static_cast<uint8_t>(i), 0x60 + i * 0x10);
    const uint8_t restoreGprs[] = {
        0x48,0x8B,0x44,0x24,0x20, 0x48,0x8B,0x4C,0x24,0x28,
        0x48,0x8B,0x54,0x24,0x30, 0x4C,0x8B,0x44,0x24,0x38,
        0x4C,0x8B,0x4C,0x24,0x40, 0x4C,0x8B,0x54,0x24,0x48,
        0x4C,0x8B,0x5C,0x24,0x50,
        0x48,0x8D,0xA4,0x24,0xD0,0x00,0x00,0x00,
    };
    memcpy(stub + p, restoreGprs, sizeof(restoreGprs)); p += sizeof(restoreGprs);
    EmitRenoStub8(stub, &p, 0x0F); EmitRenoStub8(stub, &p, 0x84);
    const size_t keepJzDisp = p; EmitRenoStub32(stub, &p, 0);

    const size_t normalLabel = p;
    const uint8_t indirectJmp[] = {0xFF,0x25,0x00,0x00,0x00,0x00};
    memcpy(stub + p, indirectJmp, sizeof(indirectJmp)); p += sizeof(indirectJmp);
    EmitRenoStub64(stub, &p, reinterpret_cast<uint64_t>(normalTarget));
    const size_t skipLabel = p;
    memcpy(stub + p, indirectJmp, sizeof(indirectJmp)); p += sizeof(indirectJmp);
    EmitRenoStub64(stub, &p, reinterpret_cast<uint64_t>(skipTarget));

    const int32_t originalJneRel = static_cast<int32_t>(normalLabel - (originalJneDisp + 4));
    const int32_t keepJzRel = static_cast<int32_t>(skipLabel - (keepJzDisp + 4));
    memcpy(stub + originalJneDisp, &originalJneRel, sizeof(originalJneRel));
    memcpy(stub + keepJzDisp, &keepJzRel, sizeof(keepJzRel));
    if (p > kCapacity) {
        VirtualFree(stub, 0, MEM_RELEASE);
        return nullptr;
    }
    if (outSize) *outSize = p;
    return stub;
}

bool InstallReno455StereoDuplicateBypass(HMODULE addon) {
    int expected = 0;
    if (!g_renoStereoBypassState.compare_exchange_strong(expected, 1,
                                                          std::memory_order_acq_rel))
        return g_renoStereoBypassState.load(std::memory_order_acquire) == 2;
    auto* base = reinterpret_cast<uint8_t*>(addon);
    uint8_t* branch = base + 0x8CAD;
    static constexpr uint8_t expectedBranch[] = {0x0F,0x84,0xBE,0x07,0x00,0x00};
    if (!PrologueEquals(branch, expectedBranch, sizeof(expectedBranch))) {
        Log("[DLSSNR-DIAG][dedup-hook] exact +0x8CAD branch mismatch/collision; bypass refused\n");
        g_renoStereoBypassState.store(-1, std::memory_order_release);
        return false;
    }

    size_t stubSize = 0;
    uint8_t* stub = BuildRenoDuplicateBranchStub(branch, base + 0x8CB3, base + 0x9471, &stubSize);
    if (!stub) {
        Log("[DLSSNR-DIAG][dedup-hook] nearby conditional thunk allocation/build failed\n");
        g_renoStereoBypassState.store(-2, std::memory_order_release);
        return false;
    }
    DWORD stubOld = 0;
    if (!VirtualProtect(stub, stubSize, PAGE_EXECUTE_READ, &stubOld)) {
        Log("[DLSSNR-DIAG][dedup-hook] conditional thunk RX protection failed error=%lu\n",
            GetLastError());
        VirtualFree(stub, 0, MEM_RELEASE);
        g_renoStereoBypassState.store(-2, std::memory_order_release);
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), stub, stubSize);

    const intptr_t relWide = reinterpret_cast<intptr_t>(stub)
        - (reinterpret_cast<intptr_t>(branch) + 5);
    if (relWide < INT32_MIN || relWide > INT32_MAX) {
        Log("[DLSSNR-DIAG][dedup-hook] nearby thunk fell outside rel32 range\n");
        VirtualFree(stub, 0, MEM_RELEASE);
        g_renoStereoBypassState.store(-2, std::memory_order_release);
        return false;
    }
    DWORD oldProtect = 0;
    if (!VirtualProtect(branch, sizeof(expectedBranch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log("[DLSSNR-DIAG][dedup-hook] branch write protection failed error=%lu\n", GetLastError());
        VirtualFree(stub, 0, MEM_RELEASE);
        g_renoStereoBypassState.store(-2, std::memory_order_release);
        return false;
    }
    branch[0] = 0xE9;
    const int32_t rel = static_cast<int32_t>(relWide);
    memcpy(branch + 1, &rel, sizeof(rel));
    branch[5] = 0x90;
    DWORD discard = 0;
    VirtualProtect(branch, sizeof(expectedBranch), oldProtect, &discard);
    FlushInstructionCache(GetCurrentProcess(), branch, sizeof(expectedBranch));

    g_renoDuplicateStub = stub;
    g_renoStereoBypassState.store(2, std::memory_order_release);
    Log("[DLSSNR-DIAG][dedup-hook] conditional stereo duplicate bypass armed branch=%p "
        "thunk=%p size=%zu normal=%p originalSkip=%p; post-takeover attributed views only\n",
        branch, stub, stubSize, base + 0x8CB3, base + 0x9471);
    return true;
}

bool ValidateReno455Image(HMODULE module) {
    __try {
        const auto* base = reinterpret_cast<const uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        return nt->Signature == IMAGE_NT_SIGNATURE
            && nt->FileHeader.TimeDateStamp == 0x6A940524u
            && nt->OptionalHeader.SizeOfImage == 0x001AC000u;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void TryInstallReno455OuterDiagnostics() {
    if (g_renoOuterState.load(std::memory_order_acquire) != 0) return;
    HMODULE addon = GetModuleHandleW(L"renodx-dlss5.addon64");
    if (!addon) return;
    int expected = 0;
    if (!g_renoOuterState.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) return;
    if (!ValidateReno455Image(addon)) {
        Log("[DLSSNR-DIAG][outer-hook] addon image identity mismatch; expected RenoDX 4.55 "
            "timestamp=0x6A940524 imageSize=0x1AC000; refused module=%p\n", addon);
        g_renoOuterState.store(-1, std::memory_order_release);
        return;
    }

    // Clean stereo build: the eligibility/create/release/state hooks below were forensic only.
    // The fix needs solely exact image/branch validation, the Streamline outer context, and the
    // conditional branch thunk. Keep the forensic implementation compiled for an evidence build,
    // but do not install any of its six MinHook detours in normal testing.
    constexpr bool kEnableRenoOuterForensics = false;
    if (!kEnableRenoOuterForensics) {
        const bool armed = InstallReno455StereoDuplicateBypass(addon);
        g_renoOuterState.store(armed ? 2 : -4, std::memory_order_release);
        Log("[DLSSNR-DIAG][clean] RenoDX outer forensics disabled; conditional stereo bypass %s\n",
            armed ? "armed" : "refused");
        return;
    }

    auto* base = reinterpret_cast<uint8_t*>(addon);
    void* targets[6] = {base + 0x303F0, base + 0x333B0, base + 0x33C70,
                        base + 0x2DE70, base + 0x2E120, base + 0x4EF0};
    void* hooks[6] = {reinterpret_cast<void*>(&HookedRenoEligible),
                      reinterpret_cast<void*>(&HookedRenoCreate0),
                      reinterpret_cast<void*>(&HookedRenoCreate1),
                      reinterpret_cast<void*>(&HookedRenoRelease0),
                      reinterpret_cast<void*>(&HookedRenoRelease1),
                      reinterpret_cast<void*>(&HookedRenoCaptureState)};
    void** originals[6] = {reinterpret_cast<void**>(&g_renoEligibleOrig),
                           reinterpret_cast<void**>(&g_renoCreateOrig[0]),
                           reinterpret_cast<void**>(&g_renoCreateOrig[1]),
                           reinterpret_cast<void**>(&g_renoReleaseOrig[0]),
                           reinterpret_cast<void**>(&g_renoReleaseOrig[1]),
                           reinterpret_cast<void**>(&g_renoCaptureStateOrig)};
    static constexpr uint8_t eligiblePrologue[] =
        {0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,
         0x48,0x81,0xEC,0xC8,0x00,0x00,0x00};
    static constexpr uint8_t createPrologue[] =
        {0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,
         0x48,0x81,0xEC,0x78,0x01,0x00,0x00};
    static constexpr uint8_t releasePrologue[] =
        {0x55,0x56,0x57,0x53,0x48,0x83,0xEC,0x38,0x48,0x8D,0x6C,0x24,0x30};
    static constexpr uint8_t capturePrologue[] =
        {0x55,0x41,0x57,0x41,0x56,0x41,0x54,0x56,0x57,0x53,0x48,0x83,0xEC,0x70};
    const bool prologuesOk =
        PrologueEquals(targets[0], eligiblePrologue, sizeof(eligiblePrologue)) &&
        PrologueEquals(targets[1], createPrologue, sizeof(createPrologue)) &&
        PrologueEquals(targets[2], createPrologue, sizeof(createPrologue)) &&
        PrologueEquals(targets[3], releasePrologue, sizeof(releasePrologue)) &&
        PrologueEquals(targets[4], releasePrologue, sizeof(releasePrologue)) &&
        PrologueEquals(targets[5], capturePrologue, sizeof(capturePrologue));
    if (!prologuesOk) {
        Log("[DLSSNR-DIAG][outer-hook] exact prologue mismatch or collision; all hooks refused\n");
        g_renoOuterState.store(-2, std::memory_order_release);
        return;
    }

    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        Log("[DLSSNR-DIAG][outer-hook] MH_Initialize failed status=%d (%s)\n",
            static_cast<int>(init), MH_StatusToString(init));
        g_renoOuterState.store(-3, std::memory_order_release);
        return;
    }
    int made = 0;
    for (; made < 6; ++made) {
        const MH_STATUS status = MH_CreateHook(targets[made], hooks[made], originals[made]);
        if (status != MH_OK) {
            Log("[DLSSNR-DIAG][outer-hook] create failed index=%d status=%d (%s); "
                "removing partial set\n", made, static_cast<int>(status),
                MH_StatusToString(status));
            break;
        }
    }
    if (made != 6) {
        while (made-- > 0) MH_RemoveHook(targets[made]);
        g_renoEligibleOrig = nullptr;
        g_renoCreateOrig[0] = g_renoCreateOrig[1] = nullptr;
        g_renoReleaseOrig[0] = g_renoReleaseOrig[1] = nullptr;
        g_renoOuterState.store(-3, std::memory_order_release);
        return;
    }

    int enabled = 0;
    for (; enabled < 6; ++enabled) {
        const MH_STATUS status = MH_EnableHook(targets[enabled]);
        if (status != MH_OK) {
            Log("[DLSSNR-DIAG][outer-hook] enable failed index=%d status=%d (%s); "
                "removing complete set\n", enabled, static_cast<int>(status),
                MH_StatusToString(status));
            break;
        }
    }
    if (enabled != 6) {
        while (enabled-- > 0) MH_DisableHook(targets[enabled]);
        for (void* target : targets) MH_RemoveHook(target);
        g_renoEligibleOrig = nullptr;
        g_renoCreateOrig[0] = g_renoCreateOrig[1] = nullptr;
        g_renoReleaseOrig[0] = g_renoReleaseOrig[1] = nullptr;
        g_renoCaptureStateOrig = nullptr;
        g_renoOuterState.store(-3, std::memory_order_release);
        return;
    }
    g_renoOuterState.store(2, std::memory_order_release);
    Log("[DLSSNR-DIAG][outer-hook] RenoDX 4.55 eligibility/create/release/state census installed "
        "module=%p predicate=%p capture=%p; read-only, exact-build only\n",
        addon, targets[0], targets[5]);
    InstallReno455StereoDuplicateBypass(addon);
}

void SetCapturedResource(ID3D12Resource*& slot, ID3D12Resource* newRes) {
    if (slot == newRes) return;
    if (newRes) newRes->AddRef();
    ID3D12Resource* old = slot;
    slot = newRes;
    if (old) old->Release();
}

bool IsLikelyComObject(const void* p) {
    if (!p) return false;
    if ((reinterpret_cast<uintptr_t>(p) & 0x7) != 0) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                       PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) == 0) return false;
    // vtable pointer at offset 0 should point into a module's code/rdata
    void* vtable = *reinterpret_cast<void* const*>(p);
    if (!vtable) return false;
    MEMORY_BASIC_INFORMATION vmbi{};
    if (VirtualQuery(vtable, &vmbi, sizeof(vmbi)) == 0) return false;
    if (vmbi.Type != MEM_IMAGE) return false;
    return true;
}

// The NGX D3D12 parameter object stores ID3D12Resource* values; once
// captured, the resources will fail an IsLikelyComObject check if the game
// frees them between frames. So we always AddRef under lock — the consumer
// only ever sees a still-valid AddRef'd pointer or null.
//
// Heuristic field discovery: we sweep a bounded window of the params struct
// looking for COM vtable pointers that are exactly TWO distinct
// ID3D12Resource pointers (MV is typically R16G16_FLOAT 16bpp ~ 1280×720;
// depth is R32_TYPELESS 32bpp at full render res). For a first iteration we
// log every COM-pointer field at runtime — the user's log will then tell us
// exact offsets. We never deref past the validated pointer.
// The original NGX-Parameter heuristic sweep is retained ONLY for future
// fallback work; CP2077 uses Streamline so the slSetTag hook above is the active
// capture path. If a future game hits NGX directly we'd resurrect this.
#if 0
void CaptureParametersFromOpaqueStruct(const void* params) {
    if (!params) return;

    constexpr size_t kSweepBytes = 0x800;
    constexpr size_t kFieldStride = sizeof(void*);
    const uint8_t* base = reinterpret_cast<const uint8_t*>(params);

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(base, &mbi, sizeof(mbi)) == 0) return;
    const uint8_t* regionEnd = static_cast<const uint8_t*>(mbi.BaseAddress) +
                                static_cast<size_t>(mbi.RegionSize);
    const size_t safeBytes = (base < regionEnd)
        ? static_cast<size_t>(regionEnd - base)
        : 0;
    const size_t sweep = (safeBytes < kSweepBytes) ? safeBytes : kSweepBytes;

    ID3D12Resource* candidateMv = nullptr;
    ID3D12Resource* candidateDepth = nullptr;
    unsigned int mvW = 0, mvH = 0, mvFmt = 0;
    unsigned int dW = 0, dH = 0;

    // Scan candidates: pointer-aligned slots whose value passes
    // IsLikelyComObject. Then probe with GetDesc — if it returns a TEXTURE2D
    // it's a real D3D12 resource. Smallest texture = motion vectors,
    // largest = scene color/depth (depending on flags).
    struct Candidate { ID3D12Resource* res; D3D12_RESOURCE_DESC desc; size_t offset; };
    Candidate cands[16] = {};
    int nCands = 0;
    for (size_t off = 0; off + kFieldStride <= sweep && nCands < 16; off += kFieldStride) {
        void* slot = *reinterpret_cast<void* const*>(base + off);
        if (!IsLikelyComObject(slot)) continue;
        ID3D12Resource* res = reinterpret_cast<ID3D12Resource*>(slot);
        // Probe via QueryInterface — if it's not really ID3D12Resource we
        // get HR != S_OK and we move on.
        ID3D12Resource* probed = nullptr;
        if (res->QueryInterface(IID_PPV_ARGS(&probed)) != S_OK || !probed) continue;
        D3D12_RESOURCE_DESC desc = probed->GetDesc();
        probed->Release();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) continue;
        cands[nCands++] = {res, desc, off};
    }

    if (nCands == 0) return;

    // Identify MV vs Depth among candidates.
    // - Depth: D-family format OR R32_TYPELESS/R32_FLOAT/R32G8X24_TYPELESS at
    //   full render resolution.
    // - Motion vectors: 2-channel float (R16G16_FLOAT=34, R32G32_FLOAT=16) at
    //   pre-DLSS resolution (smaller than depth).
    for (int i = 0; i < nCands; ++i) {
        const Candidate& c = cands[i];
        const DXGI_FORMAT f = c.desc.Format;
        const bool isDepthFmt =
            f == DXGI_FORMAT_R32_TYPELESS || f == DXGI_FORMAT_D32_FLOAT ||
            f == DXGI_FORMAT_R32_FLOAT || f == DXGI_FORMAT_R32G8X24_TYPELESS ||
            f == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
            f == DXGI_FORMAT_R24G8_TYPELESS || f == DXGI_FORMAT_D24_UNORM_S8_UINT;
        const bool isMvFmt =
            f == DXGI_FORMAT_R16G16_FLOAT || f == DXGI_FORMAT_R32G32_FLOAT ||
            f == DXGI_FORMAT_R16G16_SINT || f == DXGI_FORMAT_R32G32_SINT;
        if (isDepthFmt && !candidateDepth) {
            candidateDepth = c.res;
            dW = static_cast<unsigned int>(c.desc.Width);
            dH = c.desc.Height;
        } else if (isMvFmt && !candidateMv) {
            candidateMv = c.res;
            mvW = static_cast<unsigned int>(c.desc.Width);
            mvH = c.desc.Height;
            mvFmt = static_cast<unsigned int>(c.desc.Format);
        }
    }

    // Float / int sweep for MV scale + reset flag. Heuristic: NGX MV scale
    // is typically a non-1 finite float in [-32, 32] (engines pass
    // renderRes/displayRes ratio multiplied or sign for clip-space
    // direction). Reset is a 0/1 int. We log a fingerprint so we can
    // pinpoint exact offsets once we see it in the log.
    float mvSx = 1.0f, mvSy = 1.0f;
    int rst = 0;
    bool gotMvSx = false, gotMvSy = false;

    for (size_t off = 0; off + 4 <= sweep; off += 4) {
        const float fv = *reinterpret_cast<const float*>(base + off);
        const uint32_t iv = *reinterpret_cast<const uint32_t*>(base + off);
        if (!gotMvSx) {
            if (fv > -32.0f && fv < 32.0f && fv != 0.0f && fv != 1.0f &&
                _finite(fv)) {
                mvSx = fv;
                gotMvSx = true;
                continue;
            }
        } else if (!gotMvSy) {
            if (fv > -32.0f && fv < 32.0f && fv != 0.0f && fv != 1.0f &&
                _finite(fv)) {
                mvSy = fv;
                gotMvSy = true;
                continue;
            }
        }
        if (iv == 0u || iv == 1u) {
            rst |= static_cast<int>(iv);
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_captureMutex);
        if (candidateMv) SetCapturedResource(g_mvRes, candidateMv);
        if (candidateDepth) SetCapturedResource(g_depthRes, candidateDepth);
    }
    if (candidateMv) {
        g_mvWidth.store(mvW, std::memory_order_relaxed);
        g_mvHeight.store(mvH, std::memory_order_relaxed);
        g_mvFormat.store(mvFmt, std::memory_order_relaxed);
    }
    g_mvScaleX.store(mvSx, std::memory_order_relaxed);
    g_mvScaleY.store(mvSy, std::memory_order_relaxed);
    g_resetFlag.store(rst, std::memory_order_relaxed);

    const unsigned int n = g_evalCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (g_verboseLog && (n == 1 || (n % 600) == 0)) {
        Log("[NGX] eval#%u mv=%p %ux%u fmt=%u depth=%p %ux%u sx=%f sy=%f reset=%d nCands=%d\n",
            n, candidateMv, mvW, mvH, mvFmt, candidateDepth, dW, dH, mvSx, mvSy, rst, nCands);
    }
}
#endif // 0 — legacy NGX direct heuristic (CP2077 uses Streamline; see slSetTag hook above)

bool WriteRel32Jmp(uint8_t* dst, void* target) {
    intptr_t rel = reinterpret_cast<intptr_t>(target) -
                   (reinterpret_cast<intptr_t>(dst) + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) return false;
    DWORD oldProtect = 0;
    if (!VirtualProtect(dst, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    dst[0] = 0xE9;
    *reinterpret_cast<int32_t*>(dst + 1) = static_cast<int32_t>(rel);
    DWORD discard = 0;
    VirtualProtect(dst, 5, oldProtect, &discard);
    FlushInstructionCache(GetCurrentProcess(), dst, 5);
    return true;
}

// Allocate an executable trampoline within +/-2GB of `anchor` so an E9 from it
// can reach our hook target and the original code can JMP back to it.
// (`near` would have been clearer but it's a Windows historic macro.)
uint8_t* AllocateExecNearby(void* anchor, size_t bytes) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const uintptr_t step = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
    const uintptr_t target = reinterpret_cast<uintptr_t>(anchor);
    for (intptr_t delta = -static_cast<intptr_t>(0x40000000);
         delta <= static_cast<intptr_t>(0x40000000); delta += step) {
        uintptr_t addr = target + delta;
        addr = addr & ~(step - 1);
        void* mem = VirtualAlloc(reinterpret_cast<void*>(addr), bytes,
            MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (mem) return reinterpret_cast<uint8_t*>(mem);
    }
    return nullptr;
}

}  // namespace

// Hook on sl.interposer.dll!slSetTag — Streamline tag-registration API. CP2077
// uses Streamline (sl.interposer.dll) which proxies DLSS, so the game NEVER
// calls NVSDK_NGX_D3D12_EvaluateFeature directly (confirmed by 0 evaluations
// observed during a full play session despite DLSS being active). Streamline
// tagging is where the game explicitly labels each resource by purpose
// (BufferType: MotionVectors / Depth / ScalingInputColor / etc.) — exactly
// what we need, with NO heuristic guessing.
//
// Signature (from Streamline SDK):
//   sl::Result slSetTag(const ViewportHandle& vp,
//                       const ResourceTag* tags,
//                       uint32_t numTags,
//                       CommandBuffer* cmdBuf);
//
// ResourceTag layout (sl::ResourceTag, ~64 bytes):
//   +0x00: Resource* resource          (ptr to sl::Resource struct)
//   +0x08: BufferType type             (uint32 — see kBufferType* enum below)
//   +0x0C: ResourceLifecycle lifecycle (uint32)
//   +0x10..+0x18: Extent extent        (top/left/width/height u32)
//
// sl::Resource layout:
//   +0x00: ResourceType type           (uint32, 1 = Tex2d)
//   +0x08: void* native                (ID3D12Resource* for D3D12)
//   +0x10..: state, view, alloc bits   (we ignore)
//
// Stable across Streamline 2.x; if upstream rearranges we'll see garbage in
// the dump log and adjust.
enum SlBufferType : uint32_t {
    kBufferTypeDepth = 0,
    kBufferTypeMotionVectors = 1,
    kBufferTypeHudLessColor = 2,
    kBufferTypeUiColor = 3,
    kBufferTypeRawColor = 4,
    kBufferTypeScalingInputColor = 5,
    kBufferTypeScalingOutputColor = 6,
};

using SlSetTagFn = uint32_t (*)(const void* /*vp*/, const void* /*tags*/, uint32_t /*numTags*/, void* /*cmdBuf*/);
SlSetTagFn g_origSlSetTag = nullptr;
std::atomic<bool> g_setTagHookInstalled{false};

// THE ONE THE GAME ACTUALLY CALLS. sl.interposer exports slSetTag, slSetTagForFrame and
// slEvaluateFeature; this build of Cyberpunk tags per frame, so hooking slSetTag alone captured
// nothing for the whole life of this code -- with the closed addon present AND with it disabled.
// The addon hooks both, which is why its capture worked and ours did not.
//
//   sl::Result slSetTagForFrame(const FrameToken& frame,
//                               const ViewportHandle& vp,
//                               const ResourceTag* tags,
//                               uint32_t numTags,
//                               CommandBuffer* cmdBuf);
//
// Same payload, one extra leading argument, so the tag walk is shared and only the arity differs.
using SlSetTagForFrameFn = uint32_t (*)(const void* /*frame*/, const void* /*vp*/,
                                        const void* /*tags*/, uint32_t /*numTags*/, void* /*cmdBuf*/);
SlSetTagForFrameFn g_origSlSetTagForFrame = nullptr;
std::atomic<bool> g_setTagForFrameHookInstalled{false};
std::atomic<uint64_t> g_setTagCalls{0};
std::atomic<uint64_t> g_setTagInvalid{0};

// slEvaluateFeature is the collision-safe layer above the addon's _nvngx hooks. Unlike the signed
// feature-18 runtime, Streamline does not validate its caller's return address. Hooking here lets us
// record MAIN/VRCAM order without touching arguments, resource tags, command lists or results.
// Signature in Streamline 2.13 (and in the installed export's register use):
//   sl::Result slEvaluateFeature(sl::Feature feature,
//                                const sl::FrameToken& frame,
//                                const sl::BaseStructure** inputs,
//                                uint32_t numInputs,
//                                sl::CommandBuffer* cmdBuffer);
using SlEvaluateFeatureFn = uint32_t (*)(uint32_t /*feature*/, const void* /*frameToken*/,
                                         const void* const* /*inputs*/, uint32_t /*numInputs*/,
                                         void* /*cmdBuffer*/);
SlEvaluateFeatureFn g_origSlEvaluateFeature = nullptr;
std::atomic<bool> g_evaluateFeatureHookInstalled{false};
std::atomic<uint64_t> g_evaluateFeatureCalls{0};
std::atomic<uint64_t> g_slEvaluateBySide[3];
std::atomic<uint32_t> g_slEvaluateBurst{0};
thread_local bool t_inSlEvaluate = false;
thread_local bool t_traceSlEvaluateGpu = false;
thread_local uint64_t t_slEvaluateSequence = 0;
thread_local int t_slEvaluateSide = 2;

extern "C" int CyberpunkVR_GetSlEvaluateTraceContext(unsigned long long* sequence, int* side) {
    if (!t_inSlEvaluate || !t_traceSlEvaluateGpu) return 0;
    if (sequence) *sequence = static_cast<unsigned long long>(t_slEvaluateSequence);
    if (side) *side = t_slEvaluateSide;
    return 1;
}

extern "C" int CyberpunkVR_GetSlEvaluateCurrentContext(unsigned long long* sequence, int* side) {
    if (!t_inSlEvaluate) return 0;
    if (sequence) *sequence = static_cast<unsigned long long>(t_slEvaluateSequence);
    if (side) *side = t_slEvaluateSide;
    return 1;
}

uint64_t ReadFrameTokenWord(const void* frameToken) {
    if (!frameToken) return 0;
    __try { return *reinterpret_cast<const uint64_t*>(frameToken); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return ~uint64_t{0}; }
}

uint32_t HookedSlEvaluateFeature(uint32_t feature, const void* frameToken,
                                 const void* const* inputs, uint32_t numInputs, void* cmdBuffer) {
    if (t_inSlEvaluate)
        return g_origSlEvaluateFeature
            ? g_origSlEvaluateFeature(feature, frameToken, inputs, numInputs, cmdBuffer) : 1;

    // This outer hook is part of the safety boundary, not a census: the branch thunk must know
    // that it is executing inside an attributed MAIN/VRCAM evaluation. Do no timing, burst logging
    // or GPU-command tracing here in the clean build.
    t_inSlEvaluate = true;
    const uint64_t sequence = g_evaluateFeatureCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const int side = NrDiagSide();
    const uint64_t sideCount = g_slEvaluateBySide[side].fetch_add(1, std::memory_order_relaxed) + 1;
    if (side == 1 && sideCount == 1)
        g_nrVrcamOuterSeen.store(true, std::memory_order_relaxed);
    t_traceSlEvaluateGpu = false;
    t_slEvaluateSequence = sequence;
    t_slEvaluateSide = side;

    const uint32_t result = g_origSlEvaluateFeature
        ? g_origSlEvaluateFeature(feature, frameToken, inputs, numInputs, cmdBuffer) : 1;

    t_slEvaluateSequence = 0;
    t_slEvaluateSide = 2;
    t_inSlEvaluate = false;
    return result;
}

// Direct hook on _nvngx.dll!NVSDK_NGX_D3D12_EvaluateFeature — the
// internal NGX dispatcher. CP2077 may bypass Streamline entirely for DLSS
// and call _nvngx directly. If THIS hook also never fires, DLSS is not
// being evaluated at all in the VR config.
// Signature:
//   NVSDK_NGX_Result NVSDK_NGX_D3D12_EvaluateFeature(
//       ID3D12GraphicsCommandList* InCmdList,
//       const NVSDK_NGX_Handle* InFeatureHandle,
//       const NVSDK_NGX_Parameter* InParameters,
//       PFN_NVSDK_NGX_ProgressCallback InCallback);
using NgxD3D12EvalFn = uint32_t (*)(void*, const void*, const void*, void*);
NgxD3D12EvalFn g_origNgxEvalDirect = nullptr;
std::atomic<bool> g_ngxEvalDirectInstalled{false};
std::atomic<uint64_t> g_ngxEvalDirectCalls{0};

uint32_t HookedNgxD3D12Eval(void* cmdList, const void* handle, const void* params, void* callback) {
    const uint64_t n = g_ngxEvalDirectCalls.fetch_add(1, std::memory_order_relaxed);
    if (g_verboseLog && (n < 3 || (n % 600) == 0)) {
        Log("[NGX] _nvngx NVSDK_NGX_D3D12_EvaluateFeature invoked #%llu cmdList=%p handle=%p params=%p\n",
            static_cast<unsigned long long>(n), cmdList, handle, params);
    }
    if (g_origNgxEvalDirect) {
        return g_origNgxEvalDirect(cmdList, handle, params, callback);
    }
    return 1;
}

// LIVE-DISCOVERED layout (CP2077 + Streamline 2.x):
//   sl::ResourceTag (64 bytes, stack-allocated):
//     +0x00: NULL
//     +0x08..+0x18: GUID 4C6A5AAD-B445-496C-87FF-1AF3845BE653 (= sl::Resource type id)
//     +0x18: u32 = 1 (ResourceType::Tex2d marker)
//     +0x20: sl::Resource* (pointer to STACK-allocated Resource struct)
//     +0x28: u32 BufferType (the value we care about for MV/Depth/Color discrimination)
//     +0x2C: u32 ResourceLifecycle
//     +0x30..0x38: padding/reserved
//     +0x38: u32 extent.top
//     +0x3C: u32 extent.left
//     (extent.width/height live further but we don't need them — D3D12 has GetDesc)
//
//   sl::Resource (referenced via tag+0x20):
//     +0x00: u32 type (1=Tex2d)
//     +0x08: void* native (ID3D12Resource*)
//     +0x10+: state, view, allocator bits (we ignore)
bool ReadTagFieldsSeh(const uint8_t* tagBytes, const void*& resourceStructPtr, uint32_t& bufType) {
    __try {
        resourceStructPtr = *reinterpret_cast<const void* const*>(tagBytes + 0x20);
        bufType           = *reinterpret_cast<const uint32_t*>(tagBytes + 0x28);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool ReadResourceNativeSeh(const void* resStructPtr, const void*& outNative) {
    // LIVE-DISCOVERED from CP2077 dump:
    //   sl::Resource layout (128 bytes, stack-allocated by game caller):
    //     +0x00: NULL/padding
    //     +0x08, +0x10: GUID 4B7224183A9D70CF-61721C72F8139183 (= sl::Resource type id)
    //     +0x18: u32 = 1 (sl::ResourceType::eTex2d)
    //     +0x20: chain pointer (void* next, often NULL/stack-like)
    //     +0x28: ★ void* native (ID3D12Resource* for D3D12)
    //     +0x30, +0x38: state/view (usually NULL on tagging)
    //     +0x60..+0x78: dimensions (width/height/mip/array)
    // Three distinct native pointers observed for three distinct buffer types
    // confirms this is the per-resource discriminator.
    __try {
        outNative = *reinterpret_cast<const void* const*>(
            reinterpret_cast<const uint8_t*>(resStructPtr) + 0x28);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Raw byte dump was used to derive the exact ResourceTag + Resource layout
// from live game data. Now that the offsets are locked in (tag+0x20 → resource
// struct, +0x28 inside = native ID3D12Resource*, tag+0x28 = bufType u32),
// disable the dump entirely. Bump back to a small N to re-investigate if the
// Streamline version updates.
std::atomic<int> g_tagDumpsRemaining{0};

// Read-only stereo contract census. A generic DLSS addon can appear healthy while feeding two
// cameras through one temporal feature. Before touching behavior, prove what the game supplies:
// unique Streamline viewport/resource tuples are logged once, attributed by the engine's own
// tag+evaluate scope. Fixed storage and first-sight-only output keep this safe on the hot path.
struct TagDiagRow {
    uint64_t viewKey = 0;
    uint32_t type = 0;
    const void* resource = nullptr;
};
std::mutex g_tagDiagMutex;
std::array<TagDiagRow, 64> g_tagDiagRows{};
size_t g_tagDiagCount = 0;

const char* DlssDiagViewName(bool known, uint64_t key) {
    if (!known) return "UNKNOWN";
    if (key == 0) return "MAIN";
    if (key == CyberpunkVR_VrcamCtxKey()) return "VRCAM";
    return "OTHER";
}

void NoteTagDiag(bool viewKnown, uint64_t viewKey, const void* vp, uint32_t type,
                 ID3D12Resource* resource, const D3D12_RESOURCE_DESC& desc) {
    // The four image guides that determine whether MAIN and VRCAM really remain independent.
    if (type != kBufferTypeDepth && type != kBufferTypeMotionVectors &&
        type != kBufferTypeScalingInputColor && type != kBufferTypeScalingOutputColor) return;
    std::lock_guard<std::mutex> lock(g_tagDiagMutex);
    for (size_t i = 0; i < g_tagDiagCount; ++i) {
        const TagDiagRow& row = g_tagDiagRows[i];
        if (row.viewKey == viewKey && row.type == type && row.resource == resource) return;
    }
    if (g_tagDiagCount >= g_tagDiagRows.size()) return;
    g_tagDiagRows[g_tagDiagCount++] = TagDiagRow{viewKey, type, resource};
    Log("[DLSSNR-DIAG][tag] view=%s key=0x%llX slViewportArg=%p type=%u res=%p "
        "%llux%u fmt=%u mips=%u flags=0x%X\n",
        DlssDiagViewName(viewKnown, viewKey),
        static_cast<unsigned long long>(viewKey), vp, type, resource,
        static_cast<unsigned long long>(desc.Width), desc.Height,
        static_cast<unsigned>(desc.Format), static_cast<unsigned>(desc.MipLevels),
        static_cast<unsigned>(desc.Flags));
}

void DumpRawTagBytes(const uint8_t* tagBytes) {
    if (!g_verboseLog) return;
    int remaining = g_tagDumpsRemaining.fetch_sub(1, std::memory_order_relaxed);
    if (remaining <= 0) return;
    char hex[300] = {};
    int pos = 0;
    for (int i = 0; i < 64 && pos < static_cast<int>(sizeof(hex)) - 4; ++i) {
        __try {
            uint8_t b = tagBytes[i];
            pos += sprintf_s(hex + pos, sizeof(hex) - pos, "%02X ", static_cast<unsigned>(b));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            break;
        }
    }
    Log("[NGX-DUMP] tag raw 64B: %s\n", hex);
    // Also dump as qwords for easier pointer inspection.
    for (int q = 0; q < 8; ++q) {
        uint64_t val = 0;
        __try { val = *reinterpret_cast<const uint64_t*>(tagBytes + q * 8); }
        __except (EXCEPTION_EXECUTE_HANDLER) { break; }
        Log("[NGX-DUMP]   +0x%02x = 0x%016llx\n", q * 8,
            static_cast<unsigned long long>(val));
    }
    // ALSO dump the sl::Resource struct referenced by tag+0x20 — the native
    // ID3D12Resource* lives inside it but the offset is unclear (our +0x08
    // guess returned garbage 0x4B7224183A9D70CF on the live game). Walk first
    // 16 qwords (128 bytes) so we can see the layout.
    uint64_t resStructAddr = 0;
    __try { resStructAddr = *reinterpret_cast<const uint64_t*>(tagBytes + 0x20); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!resStructAddr) return;
    const uint8_t* res = reinterpret_cast<const uint8_t*>(resStructAddr);
    Log("[NGX-DUMP] sl::Resource @0x%016llx:\n",
        static_cast<unsigned long long>(resStructAddr));
    for (int q = 0; q < 16; ++q) {
        uint64_t val = 0;
        __try { val = *reinterpret_cast<const uint64_t*>(res + q * 8); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        const char* tag = "";
        if (val != 0 && IsLikelyComObject(reinterpret_cast<void*>(val))) {
            tag = " <COM-candidate>";
        }
        Log("[NGX-DUMP]   res+0x%02x = 0x%016llx%s\n", q * 8,
            static_cast<unsigned long long>(val), tag);
    }
}

void ProcessTag(const uint8_t* tagBytes, bool viewKnown, uint64_t viewKey, const void* vp) {
    DumpRawTagBytes(tagBytes);
    const void* resourceStructPtr = nullptr;
    uint32_t bufType = 0xFFFFFFFFu;
    if (!ReadTagFieldsSeh(tagBytes, resourceStructPtr, bufType)) {
        g_setTagInvalid.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (!resourceStructPtr) return;

    // Dereference sl::Resource.native (+0x08) to get the actual ID3D12Resource*.
    const void* nativePtr = nullptr;
    if (!ReadResourceNativeSeh(resourceStructPtr, nativePtr)) {
        g_setTagInvalid.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (!IsLikelyComObject(nativePtr)) {
        // First few invocations the resource may not yet be valid; log once.
        const uint64_t n = g_setTagCalls.load(std::memory_order_relaxed);
        if (g_verboseLog && n < 8) {
            Log("[NGX] setTag #%llu type=%u resourceStruct=%p native=%p NOT_COM\n",
                static_cast<unsigned long long>(n), bufType, resourceStructPtr, nativePtr);
        }
        g_setTagInvalid.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    ID3D12Resource* d3dRes = reinterpret_cast<ID3D12Resource*>(const_cast<void*>(nativePtr));
    // Verify via QueryInterface.
    ID3D12Resource* probed = nullptr;
    if (d3dRes->QueryInterface(IID_PPV_ARGS(&probed)) != S_OK || !probed) {
        g_setTagInvalid.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    D3D12_RESOURCE_DESC desc = probed->GetDesc();
    probed->Release();

    NoteTagDiag(viewKnown, viewKey, vp, bufType, d3dRes, desc);

    if (bufType == kBufferTypeMotionVectors) {
        std::lock_guard<std::mutex> lock(g_captureMutex);
        SetCapturedResource(g_mvRes, d3dRes);
        g_mvWidth.store(static_cast<unsigned int>(desc.Width), std::memory_order_relaxed);
        g_mvHeight.store(desc.Height, std::memory_order_relaxed);
        g_mvFormat.store(static_cast<unsigned int>(desc.Format), std::memory_order_relaxed);
    } else if (bufType == kBufferTypeDepth) {
        std::lock_guard<std::mutex> lock(g_captureMutex);
        SetCapturedResource(g_depthRes, d3dRes);
    } else if (bufType == kBufferTypeScalingInputColor) {
        std::lock_guard<std::mutex> lock(g_captureMutex);
        SetCapturedResource(g_colorRes, d3dRes);
        g_colorWidth.store(static_cast<unsigned int>(desc.Width), std::memory_order_relaxed);
        g_colorHeight.store(desc.Height, std::memory_order_relaxed);
        g_colorFormat.store(static_cast<unsigned int>(desc.Format), std::memory_order_relaxed);
    }

    // Log first ~12 tagged resources so we map every bufType value the engine
    // uses to a real D3D12 resource (width/height/format). After that, only
    // milestone logs to keep the file lean.
    const uint64_t n = g_setTagCalls.load(std::memory_order_relaxed);
    if (g_verboseLog && (n < 12 || (n % 1800) == 0)) {
        Log("[NGX] setTag #%llu bufType=%u native=%p (D3D12 %llux%u fmt=%u dim=%u)\n",
            static_cast<unsigned long long>(n),
            bufType,
            d3dRes,
            static_cast<unsigned long long>(desc.Width),
            desc.Height,
            static_cast<unsigned>(desc.Format),
            static_cast<unsigned>(desc.Dimension));
    }
}

void WalkTags(const void* vp, const void* tags, uint32_t numTags) {
    if (!tags || numTags == 0 || numTags >= 64) return;
    unsigned long long rawViewKey = 0;
    const bool viewKnown = CyberpunkVR_GetDlssEvalViewKey(&rawViewKey) != 0;
    const uint64_t viewKey = static_cast<uint64_t>(rawViewKey);
    constexpr size_t kTagStride = 64; // sl::ResourceTag size
    const uint8_t* base = reinterpret_cast<const uint8_t*>(tags);
    for (uint32_t i = 0; i < numTags; ++i) ProcessTag(base + i * kTagStride, viewKnown, viewKey, vp);
}

uint32_t HookedSlSetTagForFrame(const void* frame, const void* vp, const void* tags,
                                uint32_t numTags, void* cmdBuf) {
    const uint64_t n = g_setTagCalls.fetch_add(1, std::memory_order_relaxed);
    if (n == 0) Log("[NGX] slSetTagForFrame is the live tag path (first call: vp=%p tags=%p "
                    "numTags=%u)\n", vp, tags, numTags);
    WalkTags(vp, tags, numTags);
    if (g_origSlSetTagForFrame) return g_origSlSetTagForFrame(frame, vp, tags, numTags, cmdBuf);
    return 1;
}

uint32_t HookedSlSetTag(const void* vp, const void* tags, uint32_t numTags, void* cmdBuf) {
    const uint64_t n = g_setTagCalls.fetch_add(1, std::memory_order_relaxed);
    if (n == 0) Log("[NGX] slSetTag is the live tag path (first call: vp=%p tags=%p numTags=%u)\n",
                    vp, tags, numTags);
    // Always log first 3 calls so we can confirm the hook IS being invoked
    // even with weird arguments. After that log every 1800 calls if any.
    if (g_verboseLog && (n < 3 || (n % 1800) == 0)) {
        Log("[NGX] slSetTag invoked #%llu vp=%p tags=%p numTags=%u cmdBuf=%p\n",
            static_cast<unsigned long long>(n), vp, tags, numTags, cmdBuf);
    }
    WalkTags(vp, tags, numTags);
    if (g_origSlSetTag) {
        return g_origSlSetTag(vp, tags, numTags, cmdBuf);
    }
    return 1; // SL_RESULT_FAIL
}

// Install a 5-byte E9 inline hook on `targetFn` that JMPs into `hookFn`.
// On success, *outOriginal becomes a function pointer that can be called to
// re-enter the original function (16-byte preserved prologue + JMP back). The
// preserved-prologue scheme assumes the first 16 bytes contain only safe-to-
// relocate insns (typical for MSVC-built export entry points). Returns the
// patched bytes pointer for diagnostics, or nullptr on failure.
template <typename TFnPtr>
uint8_t* InstallE9HookAt(uint8_t* target, void* hookFn, TFnPtr* outOriginal) {
    constexpr size_t kProloguePreserve = 16;
    uint8_t* tramp = AllocateExecNearby(target, kProloguePreserve + 5);
    if (!tramp) return nullptr;

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, kProloguePreserve + 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(tramp, 0, MEM_RELEASE);
        return nullptr;
    }
    memcpy(tramp, target, kProloguePreserve);
    intptr_t backRel = reinterpret_cast<intptr_t>(target + kProloguePreserve) -
                       reinterpret_cast<intptr_t>(tramp + kProloguePreserve + 5);
    if (backRel < INT32_MIN || backRel > INT32_MAX) {
        VirtualProtect(target, kProloguePreserve + 5, oldProtect, &oldProtect);
        VirtualFree(tramp, 0, MEM_RELEASE);
        return nullptr;
    }
    tramp[kProloguePreserve + 0] = 0xE9;
    *reinterpret_cast<int32_t*>(tramp + kProloguePreserve + 1) = static_cast<int32_t>(backRel);
    *outOriginal = reinterpret_cast<TFnPtr>(tramp);
    const bool ok = WriteRel32Jmp(target, hookFn);
    VirtualProtect(target, kProloguePreserve + 5, oldProtect, &oldProtect);
    if (!ok) {
        *outOriginal = nullptr;
        VirtualFree(tramp, 0, MEM_RELEASE);
        return nullptr;
    }
    return tramp;
}

bool NgxInstallEvaluateFeatureHook() {
    if (g_setTagHookInstalled.load(std::memory_order_acquire) &&
        g_setTagForFrameHookInstalled.load(std::memory_order_acquire) &&
        g_evaluateFeatureHookInstalled.load(std::memory_order_acquire)) {
        return true;
    }
    HMODULE dll = GetModuleHandleA("sl.interposer.dll");
    if (!dll) return false;

    // MINHOOK, NOT THE HAND-ROLLED E9 HELPER. InstallE9HookAt writes a rel32 jump, which reaches
    // +/-2GB; sl.interposer.dll and this plugin land tens of gigabytes apart in a 64-bit address
    // space, so the write always failed. That is why tag capture produced nothing -- with the
    // closed addon present AND absent. MinHook allocates its trampoline near the target and is
    // already used elsewhere in this file. (It is unsafe against nvngx_dlssnr, whose signed
    // runtime validates its caller's return address; sl.interposer has no such check, which is
    // why the addon hooks it the same way.)
    static bool s_tagHooksTried = false;
    if (!s_tagHooksTried) {
        s_tagHooksTried = true;                       // one attempt, one report
        MH_STATUS mi = MH_Initialize();
        if (mi != MH_OK && mi != MH_ERROR_ALREADY_INITIALIZED) {
            Log("[NGX] MH_Initialize failed (%d); Streamline tag capture unavailable\n", (int)mi);
        } else {
            struct { const char* name; void* hook; void** orig; std::atomic<bool>* flag; } kHooks[] = {
                {"slSetTagForFrame", reinterpret_cast<void*>(&HookedSlSetTagForFrame),
                 reinterpret_cast<void**>(&g_origSlSetTagForFrame), &g_setTagForFrameHookInstalled},
                {"slSetTag", reinterpret_cast<void*>(&HookedSlSetTag),
                 reinterpret_cast<void**>(&g_origSlSetTag), &g_setTagHookInstalled},
            };
            for (auto& h : kHooks) {
                void* target = reinterpret_cast<void*>(GetProcAddress(dll, h.name));
                if (!target) { Log("[NGX] sl.interposer.dll!%s not exported\n", h.name); continue; }
                MH_STATUS c = MH_CreateHook(target, h.hook, h.orig);
                MH_STATUS e = (c == MH_OK) ? MH_EnableHook(target) : MH_ERROR_NOT_CREATED;
                if (e == MH_OK) {
                    h.flag->store(true, std::memory_order_release);
                    Log("[NGX] sl.interposer.dll!%s hooked at %p (orig=%p)\n", h.name, target, *h.orig);
                } else {
                    Log("[NGX] sl.interposer.dll!%s hook FAILED create=%d enable=%d target=%p\n",
                        h.name, (int)c, (int)e, target);
                }
            }
        }
    }


    // The earlier slEvaluateFeature probe used InstallE9HookAt's blind 16-byte displaced copy and
    // crashed on return. MinHook relocates complete instructions, but we still refuse any binary
    // except the exact installed Streamline 2.13 export and refuse an already-detoured entry. This
    // is intentionally separate from nvngx_dlssnr.dll: the signed feature-18 export remains banned.
    static bool s_evalHookTried = false;
    if (!s_evalHookTried) {
        s_evalHookTried = true;
        void* target = reinterpret_cast<void*>(GetProcAddress(dll, "slEvaluateFeature"));
        static constexpr uint8_t expected[] = {
            0x48,0x89,0x5C,0x24,0x08, 0x48,0x89,0x74,0x24,0x10,
            0x48,0x89,0x7C,0x24,0x18, 0x41,0x56, 0x48,0x83,0xEC,0x40
        };
        if (!target) {
            Log("[NGX][sl-eval] slEvaluateFeature is not exported; order census unavailable\n");
        } else if (!PrologueEquals(target, expected, sizeof(expected))) {
            Log("[NGX][sl-eval] exact Streamline 2.13 prologue mismatch or existing detour at %p; "
                "refusing to overwrite it\n", target);
        } else {
            MH_STATUS c = MH_CreateHook(target, reinterpret_cast<void*>(&HookedSlEvaluateFeature),
                                        reinterpret_cast<void**>(&g_origSlEvaluateFeature));
            MH_STATUS e = (c == MH_OK) ? MH_EnableHook(target) : MH_ERROR_NOT_CREATED;
            if (e == MH_OK) {
                g_evaluateFeatureHookInstalled.store(true, std::memory_order_release);
                Log("[NGX][sl-eval] collision-safe outer evaluation census installed at %p "
                    "(orig=%p; arguments/results unchanged)\n", target, g_origSlEvaluateFeature);
            } else {
                if (c == MH_OK) MH_RemoveHook(target);
                g_origSlEvaluateFeature = nullptr;
                Log("[NGX][sl-eval] hook FAILED create=%d enable=%d target=%p; no partial hook retained\n",
                    static_cast<int>(c), static_cast<int>(e), target);
            }
        }
    }

    return g_setTagHookInstalled.load(std::memory_order_acquire) &&
           g_evaluateFeatureHookInstalled.load(std::memory_order_acquire);
}

ID3D12Resource* NgxAcquireMotionVectors() {
    std::lock_guard<std::mutex> lock(g_captureMutex);
    if (g_mvRes) g_mvRes->AddRef();
    return g_mvRes;
}

ID3D12Resource* NgxAcquireDepth() {
    std::lock_guard<std::mutex> lock(g_captureMutex);
    if (g_depthRes) g_depthRes->AddRef();
    return g_depthRes;
}

ID3D12Resource* NgxAcquireScalingInputColor() {
    std::lock_guard<std::mutex> lock(g_captureMutex);
    if (g_colorRes) g_colorRes->AddRef();
    return g_colorRes;
}

void* NgxAcquireDepthPeek()          { return g_depthRes; }
void* NgxAcquireMotionVectorsPeek()  { return g_mvRes; }

unsigned int NgxGetColorWidth()  { return g_colorWidth.load(std::memory_order_relaxed); }
unsigned int NgxGetColorHeight() { return g_colorHeight.load(std::memory_order_relaxed); }
unsigned int NgxGetColorFormat() { return g_colorFormat.load(std::memory_order_relaxed); }

float NgxGetMvScaleX() { return g_mvScaleX.load(std::memory_order_relaxed); }
float NgxGetMvScaleY() { return g_mvScaleY.load(std::memory_order_relaxed); }
int   NgxGetResetFlag() { return g_resetFlag.load(std::memory_order_relaxed); }
unsigned int NgxGetMvWidth() { return g_mvWidth.load(std::memory_order_relaxed); }
unsigned int NgxGetMvHeight() { return g_mvHeight.load(std::memory_order_relaxed); }
unsigned int NgxGetMvFormat() { return g_mvFormat.load(std::memory_order_relaxed); }
unsigned int NgxGetEvalCount() { return g_evalCount.load(std::memory_order_relaxed); }

void NgxTryInstallDlssNrDiagnostics() {
    // The addon is loaded lazily by our host, usually one Present before its first DLSS feature
    // creation. Foveation uses the proven return-address-preserving tail path to rewrite only
    // feature-18 subrects; ordinary CALL forwarding remains prohibited.
    TryInstallReno455OuterDiagnostics();
    constexpr bool kEnableSignedRuntimeFoveation = true;
    if (!kEnableSignedRuntimeFoveation) {
        int expected = 0;
        g_nrDiagState.compare_exchange_strong(expected, -4, std::memory_order_acq_rel);
        return;
    }
    const int state = g_nrDiagState.load(std::memory_order_acquire);
    HMODULE runtime = GetModuleHandleW(L"nvngx_dlssnr.dll");
    if (!runtime) return;
    void* eval = reinterpret_cast<void*>(GetProcAddress(runtime, "NVSDK_NGX_D3D12_EvaluateFeature"));
    if (!eval) return;

    if (state == 2) {
        if (eval != g_nrTailTarget) {
            static std::atomic<bool> s_changed{false};
            bool expected = false;
            if (s_changed.compare_exchange_strong(expected, true)) {
                Log("[DLSSNR-DIAG][hook] signed runtime target changed after installation "
                    "old=%p new=%p; refusing blind rehook until restart\n", g_nrTailTarget, eval);
            }
        }
        return;
    }
    if (state != 0) return;

    int expectedState = 0;
    if (!g_nrDiagState.compare_exchange_strong(expectedState, 1, std::memory_order_acq_rel)) return;

    // Exact untouched entry bytes from signed nvngx_dlssnr.dll 310.8. Existing E9/FF25 detours
    // and future ABI changes both fail closed before MinHook sees the target.
    static constexpr uint8_t evalPrologue[] =
        {0x40,0x53,0x55,0x56,0x57,0x41,0x56,0x48,0x81,0xEC,0x60,0x02,0x00,0x00};
    if (!PrologueEquals(eval, evalPrologue, sizeof(evalPrologue))) {
        Log("[DLSSNR-DIAG][hook] EvaluateFeature prologue mismatch or existing detour; "
            "tail-jump census refused target=%p\n", eval);
        g_nrDiagState.store(-1, std::memory_order_release);
        return;
    }

    MH_STATUS mi = MH_Initialize();
    if (mi != MH_OK && mi != MH_ERROR_ALREADY_INITIALIZED) {
        Log("[DLSSNR-DIAG][hook] MH_Initialize failed status=%d\n", static_cast<int>(mi));
        g_nrDiagState.store(-2, std::memory_order_release);
        return;
    }

    uint8_t* stub = BuildNrTailStub();
    if (!stub) {
        Log("[DLSSNR-DIAG][hook] tail-jump thunk allocation failed\n");
        g_nrDiagState.store(-2, std::memory_order_release);
        return;
    }
    // MinHook validates both target and detour as executable during MH_CreateHook. Keep the hook
    // disabled while temporarily making the thunk writable again to insert its trampoline target.
    DWORD stubProtect = 0;
    if (!VirtualProtect(stub, kNrTailStubSize, PAGE_EXECUTE_READ, &stubProtect)) {
        Log("[DLSSNR-DIAG][hook] initial tail-jump thunk RX protection failed error=%lu\n",
            GetLastError());
        VirtualFree(stub, 0, MEM_RELEASE);
        g_nrDiagState.store(-2, std::memory_order_release);
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), stub, kNrTailStubSize);

    MH_STATUS create = MH_CreateHook(eval, stub, reinterpret_cast<void**>(&g_nrEvaluateOrig));
    if (create != MH_OK || !g_nrEvaluateOrig) {
        // WHERE the target sits, not merely that it failed. MinHook needs a free page within
        // reach of the target for its trampoline, and when nvngx_dlssnr.dll maps low instead of
        // into the usual 0x7FF.. range it can fail to find one. This was raised by a failure at
        // target=0x00000193441659C0 against a working run's 0x00007FFE5A7559C0; the module base
        // and the region around the target are the numbers that settle which it is.
        MEMORY_BASIC_INFORMATION mbi{};
        const SIZE_T queried = VirtualQuery(eval, &mbi, sizeof(mbi));
        Log("[DLSSNR-DIAG][hook] MinHook trampoline creation failed status=%d (%s) target=%p "
            "module=%p region=%p size=%llu state=0x%lX protect=0x%lX\n",
            static_cast<int>(create), MH_StatusToString(create), eval,
            static_cast<void*>(runtime),
            queried ? mbi.AllocationBase : nullptr,
            queried ? static_cast<unsigned long long>(mbi.RegionSize) : 0ull,
            queried ? mbi.State : 0ul,
            queried ? mbi.Protect : 0ul);
        if (create == MH_OK) MH_RemoveHook(eval);
        VirtualFree(stub, 0, MEM_RELEASE);
        g_nrEvaluateOrig = nullptr;

        // MH_ERROR_MEMORY_ALLOC IS NOT A PERMANENT REFUSAL, and latching -2 on it was wrong. It
        // says only that no page was free near the target AT THIS MOMENT. Address space moves as
        // the game loads, and NR is typically enabled long after this first attempt -- one unlucky
        // moment therefore cost a whole session its foveation, silently: NR then runs FULL FRAME
        // at roughly four times the intended GPU cost, and the coverage UI vanishes because it
        // only draws at state 2. Every other status here is a real refusal and still latches.
        if (create == MH_ERROR_MEMORY_ALLOC) {
            static std::atomic<int> s_allocRetries{0};
            const int attempt = s_allocRetries.fetch_add(1, std::memory_order_relaxed) + 1;
            constexpr int kMaxAllocRetries = 60;
            if (attempt < kMaxAllocRetries) {
                g_nrDiagState.store(0, std::memory_order_release);   // eligible again next tick
                return;
            }
            Log("[DLSSNR-DIAG][hook] giving up after %d trampoline allocation attempts; "
                "NR will run FULL FRAME with foveation inactive\n", attempt);
        }
        g_nrDiagState.store(-2, std::memory_order_release);
        return;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(stub, kNrTailStubSize, PAGE_READWRITE, &oldProtect)) {
        Log("[DLSSNR-DIAG][hook] tail-jump thunk write protection failed error=%lu\n",
            GetLastError());
        MH_RemoveHook(eval);
        VirtualFree(stub, 0, MEM_RELEASE);
        g_nrEvaluateOrig = nullptr;
        g_nrDiagState.store(-2, std::memory_order_release);
        return;
    }
    const uint64_t trampoline = reinterpret_cast<uint64_t>(g_nrEvaluateOrig);
    memcpy(stub + kNrTailTargetImmediate, &trampoline, sizeof(trampoline));
    if (!VirtualProtect(stub, kNrTailStubSize, PAGE_EXECUTE_READ, &oldProtect)) {
        Log("[DLSSNR-DIAG][hook] final tail-jump thunk RX protection failed error=%lu\n",
            GetLastError());
        MH_RemoveHook(eval);
        VirtualFree(stub, 0, MEM_RELEASE);
        g_nrEvaluateOrig = nullptr;
        g_nrDiagState.store(-2, std::memory_order_release);
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), stub, kNrTailStubSize);

    MH_STATUS enable = MH_EnableHook(eval);
    if (enable != MH_OK) {
        Log("[DLSSNR-DIAG][hook] tail-jump hook enable failed status=%d\n",
            static_cast<int>(enable));
        MH_RemoveHook(eval);
        VirtualFree(stub, 0, MEM_RELEASE);
        g_nrEvaluateOrig = nullptr;
        g_nrDiagState.store(-2, std::memory_order_release);
        return;
    }

    g_nrTailStub = stub;
    g_nrTailTarget = eval;
    g_nrDiagState.store(2, std::memory_order_release);
    const auto& preset = ActiveNrFovealPreset();
    Log("[DLSSNR-FOV] return-address-preserving feature-18 hook installed target=%p thunk=%p "
        "trampoline=%p; preset=%s, periphery-band refresh\n",
        eval, stub, reinterpret_cast<void*>(g_nrEvaluateOrig), preset.label);
}

bool NgxGetDlssNrDiagSnapshot(DlssNrDiagSnapshot* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->state = g_nrDiagState.load(std::memory_order_acquire);
    for (int i = 0; i < 3; ++i) {
        out->creates[i] = g_nrCreates[i].load(std::memory_order_relaxed);
        out->evals[i] = g_nrEvals[i].load(std::memory_order_relaxed);
        out->menuEvals[i] = g_nrMenuEvals[i].load(std::memory_order_relaxed);
        out->badResults[i] = g_nrBadResults[i].load(std::memory_order_relaxed);
        out->releases[i] = g_nrReleases[i].load(std::memory_order_relaxed);
        out->lastEvalTickMs[i] = g_nrLastEvalTick[i].load(std::memory_order_relaxed);
        out->lastHandle[i] = g_nrLastHandle[i].load(std::memory_order_relaxed);
        out->lastParams[i] = g_nrLastParams[i].load(std::memory_order_relaxed);
        out->lastResult[i] = g_nrLastResult[i].load(std::memory_order_relaxed);
        out->parameterSamples[i] = g_nrParameterSamples[i].load(std::memory_order_relaxed);
        out->parameterGetFailures[i] = g_nrParameterGetFailures[i].load(std::memory_order_relaxed);
        out->parameterValidMask[i] = g_nrParameterValidMask[i].load(std::memory_order_relaxed);
        out->intensity[i] = g_nrIntensity[i].load(std::memory_order_relaxed);
        out->localTone[i] = g_nrLocalTone[i].load(std::memory_order_relaxed);
        out->localStructure[i] = g_nrLocalStructure[i].load(std::memory_order_relaxed);
        out->skinStructure[i] = g_nrSkinStructure[i].load(std::memory_order_relaxed);
        out->motionScaleX[i] = g_nrMotionScaleX[i].load(std::memory_order_relaxed);
        out->motionScaleY[i] = g_nrMotionScaleY[i].load(std::memory_order_relaxed);
        out->preset[i] = g_nrPreset[i].load(std::memory_order_relaxed);
        out->style[i] = g_nrStyle[i].load(std::memory_order_relaxed);
        out->autoMask[i] = g_nrAutoMask[i].load(std::memory_order_relaxed);
        out->uiCorrection[i] = g_nrUiCorrection[i].load(std::memory_order_relaxed);
        out->depthInverted[i] = g_nrDepthInverted[i].load(std::memory_order_relaxed);
        out->reset[i] = g_nrReset[i].load(std::memory_order_relaxed);
        out->inputWidth[i] = g_nrInputWidth[i].load(std::memory_order_relaxed);
        out->inputHeight[i] = g_nrInputHeight[i].load(std::memory_order_relaxed);
        out->outputWidth[i] = g_nrOutputWidth[i].load(std::memory_order_relaxed);
        out->outputHeight[i] = g_nrOutputHeight[i].load(std::memory_order_relaxed);
        out->colorResource[i] = g_nrColorResource[i].load(std::memory_order_relaxed);
        out->outputResource[i] = g_nrOutputResource[i].load(std::memory_order_relaxed);
        out->motionResource[i] = g_nrMotionResource[i].load(std::memory_order_relaxed);
        out->depthResource[i] = g_nrDepthResource[i].load(std::memory_order_relaxed);
        out->fovealApplies[i] = g_nrFovealApplies[i].load(std::memory_order_relaxed);
        out->fovealCopies[i] = g_nrFovealCopies[i].load(std::memory_order_relaxed);
        out->fovealRejects[i] = g_nrFovealRejects[i].load(std::memory_order_relaxed);
    }
    out->foveationEnabled = g_nrFoveationEnabled.load(std::memory_order_relaxed) ? 1 : 0;
    out->fovealCoverage = static_cast<float>(ActiveNrFovealPreset().percent) * 0.01f;
    out->sharedHandleEvals = g_nrSharedHandleEvals.load(std::memory_order_relaxed);
    out->recursiveEvals = g_nrRecursiveEvals.load(std::memory_order_relaxed);
    out->totalEvalMicroseconds = g_nrTotalEvalUs.load(std::memory_order_relaxed);
    out->maxEvalMicroseconds = g_nrMaxEvalUs.load(std::memory_order_relaxed);
    return true;
}

int NgxGetDlssNrFovealActivePreset() {
    LoadNrFovealConfig();
    return g_nrFovealActivePreset.load(std::memory_order_relaxed);
}

int NgxGetDlssNrFovealSelectedPreset() {
    LoadNrFovealConfig();
    return g_nrFovealSelectedPreset.load(std::memory_order_acquire);
}

const char* NgxGetDlssNrFovealPresetLabel(int preset) {
    if (preset < 0 || preset >= static_cast<int>(std::size(kNrFovealPresets))) return "Unknown";
    return kNrFovealPresets[preset].label;
}

bool NgxSetDlssNrFovealSelectedPreset(int preset) {
    LoadNrFovealConfig();
    if (preset < 0 || preset >= static_cast<int>(std::size(kNrFovealPresets)) ||
        !g_nrFovealConfigPath[0]) return false;
    char value[8]{};
    _snprintf_s(value, sizeof(value), _TRUNCATE, "%d", preset);
    if (!WritePrivateProfileStringA("DLSSNRFoveation", "Preset", value,
                                    g_nrFovealConfigPath)) return false;
    g_nrFovealSelectedPreset.store(preset, std::memory_order_release);
    Log("[DLSSNR-FOV] live preset requested: %d (%s); waiting for a gameplay MAIN boundary\n",
        preset, kNrFovealPresets[preset].label);
    return true;
}
