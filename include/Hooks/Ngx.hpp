// Read-only NGX (DLSS) EvaluateFeature hook.
//
// Goal: capture the engine's per-frame Motion Vector resource (+ depth +
// scaling factors + reset flag) at the moment DLSS evaluates a frame. These
// are ground-truth velocity / depth signals the game ALREADY computes for
// DLSS, so they cost us nothing extra. We never modify NGX state — just
// AddRef captured pointers into global storage that the submit path can read
// consume for forward extrapolation (motion-vector-driven frame extrapolation
// instead of our current backward optical-flow midpoint).
//
// Second-stage goal (not in this header): per-eye DLSS history via cloned
// feature handles. Architecturally invasive — must come after we have proof
// that read-only capture is stable.
#pragma once

#include <windows.h>
#include <d3d12.h>
#include <atomic>

// Install the Streamline resource-tag capture. Safe to call repeatedly: only the first
// successful call patches; later calls are no-ops. The generic addon's later slSetTag detour may
// supersede this hook, so feature-18 parity uses the separate runtime hook below.
bool NgxInstallEvaluateFeatureHook();

// Read-only feature-18 census. Signed runtime 310.8 rejects a CALL through MinHook's trampoline,
// so this uses a register-preserving prehook that JMPs to the trampoline and leaves the addon's
// original return address untouched. It also records measured slot-1 resource Set calls without
// substituting pointers. The tail-jump has no posthook, so lastResult remains unavailable.
struct DlssNrDiagSnapshot {
    int state;                       // 0 waiting, 1 installing, 2 tail-jump active, negative = refused/failed
    unsigned long long creates[3];   // MAIN, VRCAM, UNKNOWN/OTHER
    unsigned long long evals[3];
    unsigned long long menuEvals[3];
    unsigned long long badResults[3];
    unsigned long long releases[3];
    unsigned long long sharedHandleEvals;
    unsigned long long recursiveEvals;
    unsigned long long totalEvalMicroseconds;
    unsigned long long maxEvalMicroseconds;
    unsigned long long lastEvalTickMs[3];
    uintptr_t lastHandle[3];
    uintptr_t lastParams[3];
    unsigned int lastResult[3];
    unsigned long long parameterSamples[3];
    unsigned long long parameterGetFailures[3];
    unsigned long long parameterValidMask[3];
    float intensity[3];
    float localTone[3];
    float localStructure[3];
    float skinStructure[3];
    float motionScaleX[3];
    float motionScaleY[3];
    int preset[3];
    int style[3];
    int autoMask[3];
    int uiCorrection[3];
    int depthInverted[3];
    int reset[3];
    unsigned int inputWidth[3];
    unsigned int inputHeight[3];
    unsigned int outputWidth[3];
    unsigned int outputHeight[3];
    uintptr_t colorResource[3];
    uintptr_t outputResource[3];
    uintptr_t motionResource[3];
    uintptr_t depthResource[3];
};

void NgxTryInstallDlssNrDiagnostics();
bool NgxGetDlssNrDiagSnapshot(DlssNrDiagSnapshot* out);

// Live snapshot accessors (lock-free, AddRef'd; caller must Release).
ID3D12Resource* NgxAcquireMotionVectors();
ID3D12Resource* NgxAcquireDepth();
// The pre-upscale colour DLSS SR is given. Render resolution, so it is the input a neural pass
// would use if it ran BEFORE the upscale rather than after it.
ID3D12Resource* NgxAcquireScalingInputColor();
unsigned int NgxGetColorWidth();
unsigned int NgxGetColorHeight();
unsigned int NgxGetColorFormat();
float NgxGetMvScaleX();
float NgxGetMvScaleY();
int   NgxGetResetFlag();
unsigned int NgxGetMvWidth();
unsigned int NgxGetMvHeight();
unsigned int NgxGetMvFormat();
unsigned int NgxGetEvalCount();
