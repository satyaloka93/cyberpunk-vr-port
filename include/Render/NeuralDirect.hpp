// Driving DLSSNR ourselves, per view, instead of through the closed addon.
//
// WHY. renodx-dlss5.addon64 keeps its inline resource set and its codec in SINGLE-SLOT caches at
// fixed addresses (`+0x0D510` and `+0x0C8BA`; see the DLSS5 concept). Two views can only share one
// set or thrash rebuilding it, so "neural rendering on both eyes" is closed against that binary.
// The port already captures the whole contract -- DLSSNR.Color/Output/MVec/Depth, the scalar
// parameters, and per-view keying -- so owning the call is a real alternative rather than a wish.
//
// It also lets the model run at RENDER resolution instead of output resolution. The addon's
// measured modes put the model at ~38 FPS at 3072x3072 and ~55-61 FPS at 1024x1024, so running it
// before the upscale is roughly an order of magnitude cheaper, which is what makes two eyes
// arguable at all.
//
// THIS HEADER IS STEP 1 AND DELIBERATELY DOES NOT RENDER ANYTHING. The one thing that can break
// the game outright is NGX initialisation: Cyberpunk has already initialised NGX for its own DLSS,
// and calling NVSDK_NGX_D3D12_Init* again risks disturbing that rather than only our experiment.
// So this attaches to the EXISTING context through the core's parameter accessors and reports what
// it finds. Nothing here creates a feature, allocates a target, or touches either eye's image.

#pragma once
#include <cstdint>

struct NeuralDirectStatus {
    int      enabled;             // [nrdirect] enabled in nr-direct.ini
    int      coreResolved;        // _nvngx.dll found and the lifecycle exports bound
    int      attached;            // obtained a parameter block WITHOUT calling Init
    int      ownParamsOk;         // AllocateParameters succeeded
    unsigned lastResult;          // last NVSDK_NGX_Result seen
    int      featureAvailable;    // capability probe said DLSSNR is usable (-1 = not answered)
    char     attachPath[64];      // which accessor answered: GetCapability / GetParameters
    int      createTried;         // we attempted NVSDK_NGX_D3D12_CreateFeature for id 18
    unsigned createResult;        // its NVSDK_NGX_Result
    char     note[192];
};

// Called once per frame from the present path. Cheap and lock-free once it has settled; the whole
// probe runs at most once per launch.
void NeuralDirectTick();

extern "C" int  CyberpunkVR_NeuralDirectGetStatus(NeuralDirectStatus* out);
extern "C" void CyberpunkVR_NeuralDirectSetEnabled(int enabled);
