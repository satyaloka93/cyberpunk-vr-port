---
type: Fix
title: Overlay middle-ground frame pacing
description: Remove the per-frame full GPU queue drain with exact resource fences and a previous-overlay throttle, without changing DXGI latency.
resource: https://github.com/satyaloka93/cyberpunk-vr-port/blob/psvr2-tweaks/src/vr/overlay/imgui_overlay.cpp
tags: [performance, dxgi, d3d12, frame-pacing, overlay, openxr, steamvr]
timestamp: 2026-08-10T09:08:00+09:00
---

# Outcome

The validated implementation is **Mode 2 / middle ground**:

- it does not call `IDXGISwapChain2::SetMaximumFrameLatency`;
- it does not wait on DXGI's frame-latency waitable object;
- it waits for exact allocator and Dear ImGui upload-buffer ownership;
- it waits for the immediately previous overlay submission before recording another one;
- it never performs the old current-frame full queue drain during normal rendering;
- it retains a bounded drain for resize, device replacement, and teardown.

The user observed GPU utilization increase from roughly 50–61% in the failed pacing builds to as high as **96%**. Across 98 active VRCAM samples, rendered cadence averaged **69.27 FPS** with a **67.0 FPS** median and a **49.5–89.9 FPS** range. A stable high-cadence segment averaged about **82 FPS**.

# Original failure

`OverlayRender()` submits a small Dear ImGui command list on Cyberpunk's shared D3D12 Present queue. The original code immediately signaled a fence and blocked until that signal completed:

```text
game frame work -> overlay execute -> queue signal -> CPU waits for signal -> next frame
```

A D3D12 queue signal completes only after all earlier queue work. The wait therefore drained Cyberpunk's frame as well as the overlay. CPU submission stopped while the GPU finished, then the GPU could become idle while the CPU prepared the next frame. This artificial lockstep explained the otherwise contradictory symptoms: low FPS, low CPU utilization, and low GPU utilization at the same time.

The measured pre-fix gameplay baseline was about **45.3 FPS at 3072x3072**. The external report that motivated the audit described the old drain as costing about 13 ms per Present. The precise cost varied by scene, but runtime tests consistently showed that synchronization—not allocator reset itself—was starving the GPU.

# Why a fence is still required

Removing every wait is invalid. D3D12 imposes separate lifetime rules:

| Resource | Ownership rule |
|---|---|
| Backbuffer command allocator | May be reset only after the submission recorded from that allocator has completed |
| Dear ImGui upload slot | May be overwritten only after the draw using that slot has completed |
| Shared command list and fence event | Must not be manipulated concurrently by Streamline's rotating Present workers |
| Swapchain resources during resize/teardown | Must not be released while queued overlay work still references them |

A full queue drain satisfies all four rules but is much broader than necessary. Per-resource fences satisfy the first two, explicit locking satisfies the third, and a teardown-only drain satisfies the fourth.

# The three overlay modes

These mode numbers describe **overlay policy**. They are unrelated to DXGI maximum frame latency values.

| Mode | Policy | Result |
|---|---|---|
| **0 — original** | Signal and wait immediately after every overlay submission | Long-proven and safe, but drains the current game frame and starves the GPU |
| **1 — per-resource only** | Wait only when the current backbuffer allocator or ImGui upload slot is reused | Textbook allocator safety, but queue timing became irregular; the tested unrestricted implementation ended in `DXGI_ERROR_DEVICE_HUNG` |
| **2 — middle ground** | Mode 1 exact fences plus a wait for the immediately previous overlay submission | Validated compromise: one overlay frame of overlap, regular queue timing, high GPU occupancy, no captured device fault |

Mode 2 was selected because SteamVR pacing is sensitive to consistency as well as average throughput. It avoids Mode 0's current-frame drain while preventing Mode 1 from placing small overlay submissions at increasingly unpredictable depths in the shared queue.

# Mode 2 execution sequence

The implementation lives primarily in `src/vr/overlay/imgui_overlay.cpp` and is called from `HookedPresent` in `src/vr/core/swapchain_hooks.cpp` before that Present call's OpenXR and DXGI work.

```text
lock shared overlay state
wait(previous overlay submission fence, bounded to 100 ms)
wait(current backbuffer allocator fence, bounded to 100 ms)
wait(current ImGui upload-slot fence, bounded to 100 ms)
reset allocator and shared command list
record and execute overlay draw
signal this overlay submission fence
publish signal as previousOverlayFenceValue
unlock
continue OpenXR capture/submission and DXGI Present
```

The previous-overlay signal is inserted after the overlay on Cyberpunk's shared Present queue. Waiting for it on the next overlay frame proves that all earlier shared-queue work reached that point. The CPU can overlap preparation/submission with one overlay frame, but overlay work cannot accumulate without bound.

## Fence ownership

`FrameContext::fenceValue` tracks each swapchain backbuffer allocator. `g_imguiFrameFenceValues` separately tracks Dear ImGui's rotating upload ring because its slot order is not guaranteed to match the swapchain backbuffer index. `g_previousOverlayFenceValue` provides the global one-overlay-frame throttle.

The previous-overlay fence normally subsumes older per-resource values, but the exact allocator/upload checks remain intentional defensive invariants. They protect correctness if ring ordering, skipped overlay frames, or swapchain behavior changes later.

## Present-thread serialization

Cyberpunk/Streamline presented from at least 16 worker threads during diagnostics. The old full drain accidentally serialized the single overlay command list, ImGui context, fence event, and upload state. Mode 2 makes that ownership explicit with `g_overlayMutex`.

The lock is blocking rather than `try_to_lock`. Dropping a contending overlay call would bypass the previous-overlay pacing check and could reintroduce irregular queue depth. Ownership is held through command recording, queue execution, fence signaling, and publication of the new previous-overlay value.

## Timeout and teardown behavior

Normal fence waits are bounded to 100 ms. If ownership cannot be proven, the overlay frame is skipped rather than resetting an allocator or upload buffer the GPU may still own. A fence value of `UINT64_MAX` or a failed signal disables overlay submission. Do not try to recover a genuinely hung queue by submitting more work.

Resize, device replacement, and shutdown use a separate bounded full drain before releasing command allocators, render targets, the command list, descriptor heaps, or ImGui upload resources. The timeout is 2 seconds so device removal cannot turn teardown into an infinite process hang. Both `ResizeBuffers` hooks preserve an existing `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` flag, but Mode 2 neither changes nor waits on that object.

# Rejected DXGI pacing experiments

The waitable-swapchain experiments solved the wrong layer. Microsoft recommends waiting before rendering, but an injected Present hook begins after Cyberpunk has already rendered. More importantly, the original poster's “Mode 2” meant overlay policy—not `SetMaximumFrameLatency(2)`.

| Experiment | Observed result | Decision |
|---|---|---|
| DXGI latency 1, wait at Present entry | Overlay slot waits stayed zero, but Present swung between 45 and 90 FPS and GPU utilization remained about 50–60% | Rejected; wait was phase-shifted after rendering |
| DXGI latency 1, wait after Present | Safe, but settled near the compositor's half-rate boundary: 45.07 FPS average with 14.41 ms average DXGI wait | Rejected; reduced CPU/GPU overlap |
| DXGI latency 2, wait after Present | Briefly allowed 138–180 FPS in menus, then timed out and crashed with `DXGI_ERROR_DEVICE_HUNG` | Rejected permanently for the current renderer |
| No injected DXGI wait; overlay Mode 2 | Up to 96% reported GPU use, 69.27 FPS overall active average, stable ~82 FPS segment | Current validated design |

The latency-2 crash occurred with overlay fence target `7804` completed only through `7800`. GPU breadcrumbs stopped in frame 7801 at `HologramDepth_and_Distortion` and `DecoupledParticleLighting`, both at `FinalFlushBarriers`, while work for frames 7802 and 7803 remained queued. This is consistent with unsafe overlap in Cyberpunk's shared multi-queue/VRCAM resource lifetimes. Do not raise DXGI latency to recover utilization.

# Validation evidence

## Successful Mode 2 build

```text
CyberpunkVR_Stereo.dll SHA-256
56ad555cfb5fc19f486ac34c54f7c8bbaa3fdaba9357368485b2fda0a1d6c0be
```

The successful run reported:

- `injectedDxgiWait=0`;
- `mode=previous-overlay-fence`;
- `overlayPacing=1` for all 106 performance samples;
- 98 active VRCAM samples at 69.27 FPS average and 67.0 FPS median;
- a stable high-cadence segment around 82 FPS;
- `previousOverlayWait=5.99 ms` average across active gameplay;
- `overlaySlotWait=0.00 ms` throughout;
- GPU utilization as high as 96%, reported by the user;
- at least 37,800 XR cycles with only two startup misses;
- normal OpenXR session shutdown;
- no previous-overlay timeout, allocator fence failure, DRED fault, device removal, or new crash report.

Application-side XR cadence varied with scene load once the renderer became GPU-fed; Mode 2 does not promise 90 newly rendered frames in every scene. Headset smoothness remains an experiential acceptance check.

## Preserved logs and rollback

| Artifact | SHA-256 | Meaning |
|---|---|---|
| `build/test-logs/cyberpunkvrport-overlay-middle-ground-success-20260810-0901.log` | `ae3f98d02db0eab2b442c7b3a9732f343774006941349b6b63d1685e00c38f3e` | Successful Mode 2 run |
| `build/test-logs/cyberpunkvrport-post-reboot-latency1-20260810-0848.log` | `e88ab817f3731e31a43f392e4c9f14b0a74addb849536b5085f683eb18468ae0` | Post-Present DXGI latency-1 comparison, about 39.78 FPS over its last 30 gameplay samples |
| `build/test-logs/cyberpunkvrport-post-present-latency2-crash-20260810-0825.log` | `200bfcad09038fb166dc9b08887367a3b71136b6077fbf504ccc0e11514c93cf` | Rejected DXGI latency-2 crash |
| `build/backups/CyberpunkVR_Stereo.pre-overlay-middle-ground-20260810-0849.dll` | `6d07ed0350fa27629fd64dd8500c2df1f81dab21d12028a54505f8ec6d8d8edb` | Known-safe but slow latency-1 rollback |

The earlier unrestricted per-resource experiment is preserved as `build/backups/overlay-frame-fences-failed-20260809.patch` and must not be restored as current behavior.

# Reading performance diagnostics

Current performance lines use:

```text
[PERF] present=... xr=... vrcam=... overlayPacing=1
       previousOverlayWait=... overlaySlotWait=...
```

- `present` is the observed Present-hook cadence.
- `vrcam` approximates newly rendered second-eye cadence and is the best internal new-frame signal.
- `xr` is application-side OpenXR cycle cadence, not necessarily the panel refresh rate.
- `previousOverlayWait` is the intended Mode 2 throttle and can increase when the GPU is busy.
- `overlaySlotWait` measures exact allocator/upload-slot reuse; sustained nonzero values indicate a resource-ring problem.

fpsVR displayed implausible 1–2 ms frame times during DXGI latency-1 tests while the plugin measured real 22–27 ms rendered-frame intervals. External overlays may hook a different Present boundary and should not be treated as authoritative for this injected VRCAM/OpenXR path. GPU utilization remains useful as a broad signal when paired with internal Present/VRCAM cadence.

# Regression checks

1. Confirm startup logs show `injectedDxgiWait=0` and `mode=previous-overlay-fence`.
2. Load gameplay more than once; loading transitions must not produce a previous-overlay timeout.
3. Open and close F10 controls while watching for command-list or ImGui fence errors.
4. Perform an alt-tab cycle if that workflow is used.
5. If display mode or size changes, confirm `ResizeBuffers` completes and overlay resources reinitialize.
6. Exit normally and confirm the OpenXR submit thread stops cleanly.
7. Stop immediately on corruption, a freeze, driver reset, device removal, or any new `DXGI_ERROR_DEVICE_HUNG`.

Related limitation: [ray-traced stereo resources](../architecture/ray-tracing-stereo-limitations.md).

# Citations

[1] [Microsoft — Reduce latency with DXGI waitable swap chains](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model)
[2] [Microsoft — `IDXGISwapChain2::SetMaximumFrameLatency`](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-setmaximumframelatency)
