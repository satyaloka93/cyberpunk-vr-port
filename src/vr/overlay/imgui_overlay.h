#pragma once

#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>

void OverlaySetDeviceAndQueue(ID3D12Device* device, ID3D12CommandQueue* queue);
void OverlaySetWindow(HWND hwnd);
void OverlayRender(IDXGISwapChain* swapChain);
// Call immediately after the real Present returns, on the same thread that called it.
// Feeds the [BBIDX-DIAG] backbuffer-reuse check in OverlayRender.
void OverlayNotifyPresentCompleted();
// Which overlay pacing arm this binary was built with. Logged at startup and per [PERF] line.
const char* OverlayPacingModeName();
// Open the load-transition guard: for a bounded window the overlay reverts to a full queue
// drain. Call on save-load transitions, where every recorded GPU hang occurred.
void OverlayArmLoadGuard(const char* reason);
void OverlayInvalidateSwapchainResources();
bool OverlayIsVisible();
