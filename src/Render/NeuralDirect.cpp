// See include/Render/NeuralDirect.hpp for why the port would drive DLSSNR itself.
//
// Step 1: attach and report. No feature is created, no resource is allocated, nothing is drawn.

#include <windows.h>
#include <d3d12.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <atomic>
#include <share.h>

#include "Render/NeuralDirect.hpp"

extern void Log(const char* fmt, ...);
extern char g_liveControlPath[MAX_PATH];          // vrport.ini; our ini is its sibling
extern "C" ID3D12Device* CyberpunkVR_GetGameDevice();
extern "C" ID3D12CommandQueue* CyberpunkVR_GetGameQueue();
unsigned int NgxGetColorWidth();
unsigned int NgxGetColorHeight();
unsigned int NgxGetMvWidth();
unsigned int NgxGetMvHeight();
// Peek without AddRef -- purely for the diagnostic line; the probe never dereferences these.
void* NgxAcquireDepthPeek();
void* NgxAcquireMotionVectorsPeek();


namespace {

// The NVSDK_NGX_Parameter ABI, duplicated rather than shared: Ngx.cpp keeps its copy in an
// anonymous namespace, and prying that open to reach two getters would restructure a file this
// experiment has no business touching. Same layout (eight Set overloads, then Get ULL/F/D/UI/I/...)
// and the same SEH containment, because the block is supplied by code we do not own.
bool NgxGetUInt(const void* params, const char* name, uint32_t* out) {
    if (!params || !name || !out) return false;
    uint32_t r = 0xFFFFFFFFu;
    __try {
        void** vt = *reinterpret_cast<void***>(const_cast<void*>(params));
        using Fn = uint32_t(__fastcall*)(const void*, const char*, uint32_t*);
        r = reinterpret_cast<Fn>(vt[11])(params, name, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return (r & 0x80000000u) == 0;
}

// The Set half of the same vtable: eight setters at 0..7 (ULL/F/D/UI/I/D3D11/D3D12/void*),
// then the getters at 8..15. Only UI and I are needed to describe a feature.
void NgxSetUInt(void* params, const char* name, uint32_t v) {
    if (!params || !name) return;
    __try {
        void** vt = *reinterpret_cast<void***>(params);
        reinterpret_cast<void(__fastcall*)(void*, const char*, uint32_t)>(vt[3])(params, name, v);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void NgxSetInt(void* params, const char* name, int v) {
    if (!params || !name) return;
    __try {
        void** vt = *reinterpret_cast<void***>(params);
        reinterpret_cast<void(__fastcall*)(void*, const char*, int)>(vt[4])(params, name, v);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

bool NgxGetInt(const void* params, const char* name, int* out) {
    if (!params || !name || !out) return false;
    uint32_t r = 0xFFFFFFFFu;
    __try {
        void** vt = *reinterpret_cast<void***>(const_cast<void*>(params));
        using Fn = uint32_t(__fastcall*)(const void*, const char*, int*);
        r = reinterpret_cast<Fn>(vt[12])(params, name, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return (r & 0x80000000u) == 0;
}

// ---- the NGX core lifecycle --------------------------------------------------------------------
// Deliberately NOT Init/Init_Ext/Init_ProjectID. The game has already initialised NGX for its own
// DLSS; a second initialisation is the one call here that can break the game rather than only the
// experiment. Both accessors below read the context that already exists.
using PFN_GetCapabilityParameters = uint32_t (*)(void** outParams);
using PFN_GetParameters           = uint32_t (*)(void** outParams);
using PFN_AllocateParameters      = uint32_t (*)(void** outParams);
using PFN_DestroyParameters       = uint32_t (*)(void* params);
using PFN_CreateFeature  = uint32_t (*)(ID3D12GraphicsCommandList*, uint32_t featureId,
                                        void* params, void** outHandle);
using PFN_ReleaseFeature = uint32_t (*)(void* handle);
using PFN_GetScratchBufferSize = uint32_t (*)(uint32_t featureId, const void* params, size_t* out);

PFN_GetCapabilityParameters g_getCapability = nullptr;
PFN_GetParameters           g_getParameters = nullptr;
PFN_AllocateParameters      g_allocParams   = nullptr;
PFN_DestroyParameters       g_destroyParams = nullptr;
PFN_CreateFeature           g_createFeature = nullptr;
PFN_ReleaseFeature          g_releaseFeature = nullptr;
PFN_GetScratchBufferSize    g_getScratchSize = nullptr;
// The SNIPPET's own exports. _nvngx.dll is the dispatcher: it maps a feature id onto a snippet,
// and it refuses feature 18 for this app's context -- GetScratchBufferSize(18), which allocates
// nothing and reads almost no parameters, fails with the same 0xBAD0000C as creation. Calling
// nvngx_dlssnr.dll directly skips that dispatch. This is a CALL, not a hook: the return-address
// validation that makes trampolines over its prologue return 0xBAD00002 does not apply.
PFN_CreateFeature           g_snippetCreate = nullptr;
PFN_GetScratchBufferSize    g_snippetScratch = nullptr;
PFN_ReleaseFeature          g_snippetRelease = nullptr;
int      g_createTried = 0;
bool     g_createDone = false;
unsigned g_createResult = 0;

bool     g_enabled = false;
bool     g_done = false;               // the ATTACH probe has run (not: this tick is finished)
int      g_coreResolved = 0;
int      g_attached = 0;
int      g_ownParamsOk = 0;
int      g_featureAvailable = -1;
unsigned g_lastResult = 0;
char     g_attachPath[64] = {};
char     g_note[192] = {};
char     g_iniPath[MAX_PATH] = {};

void SetNote(const char* fmt, ...) {
    va_list a; va_start(a, fmt);
    _vsnprintf_s(g_note, sizeof(g_note), _TRUNCATE, fmt, a);
    va_end(a);
}

void ResolveIniPath() {
    if (g_iniPath[0]) return;
    strncpy_s(g_iniPath, MAX_PATH, g_liveControlPath, _TRUNCATE);
    char* slash = strrchr(g_iniPath, '\\');
    if (slash) { slash[1] = '\0'; strncat_s(g_iniPath, MAX_PATH, "nr-direct.ini", _TRUNCATE); }
    else       { strncpy_s(g_iniPath, MAX_PATH, "nr-direct.ini", _TRUNCATE); }
}

void LoadIni() {
    ResolveIniPath();
    if (FILE* f = _fsopen(g_iniPath, "r", _SH_DENYNO)) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            int v = 0;
            if (sscanf_s(line, "enabled=%d", &v) == 1 || sscanf_s(line, "enabled = %d", &v) == 1)
                g_enabled = (v != 0);
        }
        fclose(f);
        return;
    }
    // Seed it, so the switch is discoverable rather than a key you have to be told about.
    if (FILE* f = _fsopen(g_iniPath, "w", _SH_DENYNO)) {
        fprintf(f, "; Direct DLSSNR experiment -- the port driving neural rendering itself,\n"
                   "; per view, instead of through renodx-dlss5.addon64.\n"
                   "; Step 1 only attaches to the existing NGX context and reports. It creates no\n"
                   "; feature, allocates no target and draws nothing.\n"
                   "[nrdirect]\nenabled=0\n");
        fclose(f);
    }
}

void SaveIni() {
    ResolveIniPath();
    if (FILE* f = _fsopen(g_iniPath, "w", _SH_DENYNO)) {
        fprintf(f, "[nrdirect]\nenabled=%d\n", g_enabled ? 1 : 0);
        fclose(f);
    }
}

bool ResolveCore() {
    HMODULE core = GetModuleHandleW(L"_nvngx.dll");
    if (!core) return false;
    g_getCapability = reinterpret_cast<PFN_GetCapabilityParameters>(
        GetProcAddress(core, "NVSDK_NGX_D3D12_GetCapabilityParameters"));
    g_getParameters = reinterpret_cast<PFN_GetParameters>(
        GetProcAddress(core, "NVSDK_NGX_D3D12_GetParameters"));
    g_allocParams = reinterpret_cast<PFN_AllocateParameters>(
        GetProcAddress(core, "NVSDK_NGX_D3D12_AllocateParameters"));
    g_destroyParams = reinterpret_cast<PFN_DestroyParameters>(
        GetProcAddress(core, "NVSDK_NGX_D3D12_DestroyParameters"));
    g_createFeature = reinterpret_cast<PFN_CreateFeature>(
        GetProcAddress(core, "NVSDK_NGX_D3D12_CreateFeature"));
    g_releaseFeature = reinterpret_cast<PFN_ReleaseFeature>(
        GetProcAddress(core, "NVSDK_NGX_D3D12_ReleaseFeature"));
    g_getScratchSize = reinterpret_cast<PFN_GetScratchBufferSize>(
        GetProcAddress(core, "NVSDK_NGX_D3D12_GetScratchBufferSize"));
    // LOAD IT OURSELVES IF NOBODY ELSE HAS. With the addon disabled nothing in the process ever
    // asks for neural rendering, so the snippet is absent -- and the core cannot dispatch feature
    // 18 to a snippet that was never loaded. That, not a context or permissions problem, is what
    // 0xBAD0000C meant on both the query and the create. Loading it is exactly what bypassing the
    // addon has to mean.
    HMODULE snip = GetModuleHandleW(L"nvngx_dlssnr.dll");
    if (!snip) {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);          // the game exe -> bin\x64
        if (char* slash = strrchr(path, '\\')) slash[1] = '\0';
        strncat_s(path, MAX_PATH, "nvngx_dlssnr.dll", _TRUNCATE);
        snip = LoadLibraryA(path);
        Log("[nrdirect] snippet not present; LoadLibrary(\"%s\") -> %p (error %lu)\n",
            path, (void*)snip, snip ? 0UL : GetLastError());
    }
    if (snip) {
        g_snippetCreate = reinterpret_cast<PFN_CreateFeature>(
            GetProcAddress(snip, "NVSDK_NGX_D3D12_CreateFeature"));
        g_snippetScratch = reinterpret_cast<PFN_GetScratchBufferSize>(
            GetProcAddress(snip, "NVSDK_NGX_D3D12_GetScratchBufferSize"));
        g_snippetRelease = reinterpret_cast<PFN_ReleaseFeature>(
            GetProcAddress(snip, "NVSDK_NGX_D3D12_ReleaseFeature"));
        Log("[nrdirect] nvngx_dlssnr.dll direct exports: Create=%p Scratch=%p Release=%p\n",
            (void*)g_snippetCreate, (void*)g_snippetScratch, (void*)g_snippetRelease);
    } else {
        Log("[nrdirect] nvngx_dlssnr.dll not loaded by name; snippet-direct path unavailable\n");
    }
    g_coreResolved = (g_getCapability || g_getParameters) && g_allocParams ? 1 : 0;
    Log("[nrdirect] _nvngx.dll resolved: GetCapabilityParameters=%p GetParameters=%p "
        "AllocateParameters=%p DestroyParameters=%p\n",
        (void*)g_getCapability, (void*)g_getParameters,
        (void*)g_allocParams, (void*)g_destroyParams);
    return g_coreResolved != 0;
}

// The capability key for feature 18 is not documented anywhere available, so it is probed the same
// way every other unknown here has been: ask for several plausible names and report which answer.
void ProbeAvailability(void* params) {
    static const char* kNames[] = {
        "DLSSNR.Available", "NVSDK_NGX_Parameter_DLSSNR_Available",
        "DLSSD.Available",  "SuperSampling.Available",
        "DLSSNR.NeedsUpdatedDriver", "DLSSNR.FeatureInitResult",
    };
    for (const char* n : kNames) {
        int i = 0; uint32_t u = 0;
        if (NgxGetInt(params, n, &i))       { Log("[nrdirect]   %s = %d\n", n, i);
                                                  if (strstr(n, "DLSSNR") || strstr(n, "DLSSD")) g_featureAvailable = i ? 1 : 0; }
        else if (NgxGetUInt(params, n, &u)) { Log("[nrdirect]   %s = %u\n", n, u);
                                                  if (strstr(n, "DLSSNR") || strstr(n, "DLSSD")) g_featureAvailable = u ? 1 : 0; }
    }
}


// ---- creating feature 18 ourselves --------------------------------------------------------------
// This is the availability answer the capability probe could not give: only SuperSampling.Available
// responded, and that is DLSS SR, not DLSSNR. CreateFeature either accepts feature 18 with our own
// parameter block or returns a result saying why.
//
// It records on OUR OWN command list and touches NO game resource. Evaluation is deliberately a
// separate step: the guides belong to the engine, and transitioning a resource Cyberpunk owns is
// documented in this tree as a device-removal, not a warning.
// Returns false while the preconditions are not met, so the caller can retry. EVERY bail names
// itself: the first cut returned silently three times over and produced a run with the attach
// probe logged and the creation attempt invisible, which is the third time that pattern has cost
// a session here.
bool TryCreateFeature18() {
    ID3D12Device* dev = CyberpunkVR_GetGameDevice();
    ID3D12CommandQueue* queue = CyberpunkVR_GetGameQueue();
    const unsigned w = NgxGetColorWidth()  ? NgxGetColorWidth()  : NgxGetMvWidth();
    const unsigned h = NgxGetColorHeight() ? NgxGetColorHeight() : NgxGetMvHeight();

    const char* blocker = nullptr;
    if (!g_createFeature || !g_allocParams) blocker = "CreateFeature/AllocateParameters not resolved";
    else if (!dev)   blocker = "no game device";
    else if (!queue) blocker = "no game command queue";
    else if (!w || !h) blocker = "render resolution not captured (no ScalingInputColor or MV tag yet)";
    if (blocker) {
        // ON CHANGE *AND* PERIODICALLY. Logging only on change printed this once, before any tag
        // had arrived, and then never again -- so the line on record showed colour=0x0 mv=0x0
        // long after capture had started, and there was no way to tell a cleared blocker from a
        // stuck one. Same mistake as the two before it: a diagnostic that cannot distinguish
        // "still waiting" from "no longer looking".
        // Fires on a changed blocker, on ANY change in the captured values, or every 120 ticks.
        // 600 was longer than these sessions last, so the only line on record was still the very
        // first one -- the same staleness this was meant to fix, one order of magnitude smaller.
        static const char* s_lastBlocker = nullptr;
        static uint32_t s_ticks = 0, s_lastSig = 0xFFFFFFFFu;
        const uint32_t sig = NgxGetColorWidth() * 73856093u ^ NgxGetMvWidth() * 19349663u ^
                             static_cast<uint32_t>(reinterpret_cast<uintptr_t>(NgxAcquireMotionVectorsPeek()));
        ++s_ticks;
        if (s_lastBlocker != blocker || s_lastSig != sig || (s_ticks % 120) == 0) {
            s_lastBlocker = blocker;
            s_lastSig = sig;
            Log("[nrdirect] create waiting (tick %u): %s | device=%p queue=%p colour=%ux%u "
                "mv=%ux%u depth=%p mvres=%p\n", s_ticks, blocker, (void*)dev, (void*)queue,
                NgxGetColorWidth(), NgxGetColorHeight(), NgxGetMvWidth(), NgxGetMvHeight(),
                (void*)NgxAcquireDepthPeek(), (void*)NgxAcquireMotionVectorsPeek());
        }
        return false;
    }

    ID3D12CommandAllocator* alloc = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    void* params = nullptr;
    void* handle = nullptr;
    g_createTried = 1;

    if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc))) ||
        FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr,
                                      IID_PPV_ARGS(&list))) ||
        FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
        SetNote("could not create our own allocator/list/fence");
        Log("[nrdirect] %s\n", g_note);
        if (fence) fence->Release();
        if (list)  list->Release();
        if (alloc) alloc->Release();
        return true;                              // a real failure, not a precondition -- stop retrying
    } else {
        g_lastResult = g_allocParams(&params);
        if (params) {
            // NAMES TAKEN FROM THE RUNTIME'S OWN STRING TABLE, not extrapolated from the addon's
            // config keys. The first attempt used DLSSNR.InputWidth / DLSSNR.CreationNodeMask and
            // was refused with 0xBAD0000C: nvngx_dlssnr knows DLSSNR.Width / DLSSNR.Height, and the
            // node masks and quality value are BARE, with no DLSSNR prefix at all.
            NgxSetUInt(params, "DLSSNR.Width",  w);
            NgxSetUInt(params, "DLSSNR.Height", h);
            NgxSetInt(params,  "DLSSNR.Enabled", 1);
            NgxSetInt(params,  "DLSSNR.Reset",   1);
            NgxSetInt(params,  "CreationNodeMask",   1);
            NgxSetInt(params,  "VisibilityNodeMask", 1);
            NgxSetInt(params,  "PerfQualityValue",   0);
            NgxSetInt(params,  "DLSSNR.Hint.Render.Preset", 0);
            // Render resolution in and out -- the whole point is the model BEFORE the upscale, so
            // the subrects describe the full 1:1 region rather than any scaling.
            NgxSetUInt(params, "DLSSNR.ColorSubrectBaseX", 0);
            NgxSetUInt(params, "DLSSNR.ColorSubrectBaseY", 0);
            NgxSetUInt(params, "DLSSNR.ColorSubrectWidth",  w);
            NgxSetUInt(params, "DLSSNR.ColorSubrectHeight", h);
            NgxSetUInt(params, "DLSSNR.OutputSubrectWidth",  w);
            NgxSetUInt(params, "DLSSNR.OutputSubrectHeight", h);

            // Exported and previously unused. DLSS-family features are normally given a scratch
            // buffer at creation, and omitting one is a plausible cause of "unable to initialise".
            size_t scratch = 0;
            if (g_getScratchSize) {
                const uint32_t sr = g_getScratchSize(18u, params, &scratch);
                Log("[nrdirect] core GetScratchBufferSize(18) -> %zu bytes result=0x%X\n", scratch, sr);
            }
            if (g_snippetScratch) {
                size_t ss = 0;
                const uint32_t sr = g_snippetScratch(18u, params, &ss);
                Log("[nrdirect] snippet GetScratchBufferSize(18) -> %zu bytes result=0x%X\n", ss, sr);
            }

            g_createResult = g_createFeature(list, 18u, params, &handle);
            Log("[nrdirect] core CreateFeature(id=18) -> handle=%p result=0x%X\n",
                handle, g_createResult);
            if (!handle && g_snippetCreate) {
                const uint32_t sr = g_snippetCreate(list, 18u, params, &handle);
                Log("[nrdirect] snippet CreateFeature(id=18) -> handle=%p result=0x%X\n", handle, sr);
                if (handle) g_createResult = sr;
            }

        } else {
            Log("[nrdirect] AllocateParameters for creation failed result=0x%X\n", g_lastResult);
        }

        list->Close();
        ID3D12CommandList* lists[] = { list };
        queue->ExecuteCommandLists(1, lists);
        queue->Signal(fence, 1);
        if (fence->GetCompletedValue() < 1) {
            HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (ev) { fence->SetEventOnCompletion(1, ev); WaitForSingleObject(ev, 2000); CloseHandle(ev); }
        }
        SetNote("feature 18 create %s (0x%X) at %ux%u",
                handle ? "ACCEPTED" : "REFUSED", g_createResult, w, h);
        Log("[nrdirect] %s\n", g_note);
    }

    if (handle) {
        if (g_snippetRelease) g_snippetRelease(handle);
        else if (g_releaseFeature) g_releaseFeature(handle);
    }
    if (params && g_destroyParams) g_destroyParams(params);
    if (fence) fence->Release();
    if (list)  list->Release();
    if (alloc) alloc->Release();
    return true;
}

void RunProbeOnce() {
    if (!ResolveCore()) { SetNote("_nvngx.dll not loaded, or the lifecycle exports are missing"); return; }

    void* params = nullptr;
    if (g_getCapability) {
        g_lastResult = g_getCapability(&params);
        if (params) strncpy_s(g_attachPath, sizeof(g_attachPath), "GetCapabilityParameters", _TRUNCATE);
    }
    if (!params && g_getParameters) {
        g_lastResult = g_getParameters(&params);
        if (params) strncpy_s(g_attachPath, sizeof(g_attachPath), "GetParameters", _TRUNCATE);
    }
    if (!params) {
        SetNote("no parameter block from the existing context (result 0x%X); a second "
                "NVSDK_NGX_D3D12_Init would be required, which is what this step refuses to do",
                g_lastResult);
        Log("[nrdirect] %s\n", g_note);
        return;
    }
    g_attached = 1;
    Log("[nrdirect] attached to the existing NGX context via %s (params=%p, result=0x%X)\n",
        g_attachPath, params, g_lastResult);
    ProbeAvailability(params);

    // Our own block, which a real evaluate would populate. Allocated and immediately released --
    // holding one across the game's own NGX work is a risk this step has no reason to take.
    void* mine = nullptr;
    g_lastResult = g_allocParams(&mine);
    g_ownParamsOk = mine ? 1 : 0;
    Log("[nrdirect] AllocateParameters -> params=%p result=0x%X\n", mine, g_lastResult);
    if (mine && g_destroyParams) g_destroyParams(mine);

    SetNote("attached via %s; own parameter block %s; feature-18 availability %s",
            g_attachPath, g_ownParamsOk ? "OK" : "FAILED",
            g_featureAvailable < 0 ? "not answered by any probed key"
                                   : (g_featureAvailable ? "reported available" : "reported unavailable"));
    Log("[nrdirect] %s\n", g_note);
}

} // namespace

void NeuralDirectTick() {
    static std::atomic<bool> s_loaded{false};
    bool expected = false;
    if (s_loaded.compare_exchange_strong(expected, true)) {
        LoadIni();
        Log("[nrdirect] gate: enabled=%d ini=%s\n", g_enabled ? 1 : 0, g_iniPath);
    }
    // NOT `|| g_done`. g_done means "the attach probe has run", and gating the whole tick on it
    // short-circuited the function on the second frame -- so the creation retry below, the entire
    // point of the retry loop, ran exactly once, before any tag existed. The probe keeps its own
    // guard further down; this one is only about being switched off.
    if (!g_enabled) return;

    // EVERY GATE ANNOUNCES ITSELF. The first cut returned silently on each of these, and a session
    // with NGX up, the addon registered and enabled=1 produced no output at all -- leaving nothing
    // to distinguish "waiting" from "wired wrong". That is the same defect that made the feature-18
    // census unreadable, so it does not get repeated here.
    static std::atomic<uint32_t> s_waits{0};
    ID3D12Device* dev = CyberpunkVR_GetGameDevice();
    HMODULE core = GetModuleHandleW(L"_nvngx.dll");
    if (!dev || !core) {
        const uint32_t n = s_waits.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n == 1 || n == 30 || n == 300 || n == 3000) {
            Log("[nrdirect] waiting (tick %u): game device=%p _nvngx.dll=%p "
                "nvngx_dlssnr.dll=%p sl.interposer=%p\n", n, (void*)dev, (void*)core,
                (void*)GetModuleHandleW(L"nvngx_dlssnr.dll"),
                (void*)GetModuleHandleW(L"sl.interposer.dll"));
        }
        return;
    }
    if (!g_done) {
        g_done = true;
        Log("[nrdirect] gates passed: device=%p _nvngx.dll=%p -- probing.\n", (void*)dev, (void*)core);
        RunProbeOnce();
    }
    // Creation needs things the attach probe does not: a command queue, and a render resolution
    // that only exists once the engine has tagged a DLSS input. Both arrive later than the device,
    // so this retries every frame until it succeeds or gives a reason.
    if (!g_createDone) g_createDone = TryCreateFeature18();
}

extern "C" int CyberpunkVR_NeuralDirectGetStatus(NeuralDirectStatus* out) {
    if (!out) return 0;
    out->enabled          = g_enabled ? 1 : 0;
    out->coreResolved     = g_coreResolved;
    out->attached         = g_attached;
    out->ownParamsOk      = g_ownParamsOk;
    out->lastResult       = g_lastResult;
    out->featureAvailable = g_featureAvailable;
    out->createTried      = g_createTried;
    out->createResult     = g_createResult;
    strncpy_s(out->attachPath, sizeof(out->attachPath), g_attachPath, _TRUNCATE);
    strncpy_s(out->note, sizeof(out->note), g_note, _TRUNCATE);
    return 1;
}

extern "C" void CyberpunkVR_NeuralDirectSetEnabled(int enabled) {
    g_enabled = (enabled != 0);
    SaveIni();
    Log("[nrdirect] %s; the probe runs once, on the next launch.\n",
        g_enabled ? "enabled" : "disabled");
}
