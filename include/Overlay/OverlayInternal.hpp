#pragma once

// ================================================================================================
// What the overlay's files hand each other, after ImGuiOverlay.cpp was split by subject.
//
// Everything here is inside `namespace overlay`, which is what the file's old anonymous namespace
// became. Same rule as the other internal headers: a name belongs here when a file OTHER than the one
// defining it uses it.
// ================================================================================================

#include "Runtimes/OpenXRManager.hpp"

#include <imgui.h>
#include <im3d.h>

#include <windows.h>
#include <d3d12.h>
#include <cstdint>

namespace overlay {


Im3d::Vec3 AbstractHandPointToHeadSpace(const OpenXRHeadPose& handPose, bool isLeftHand, float hx, float hy, float hz);
bool DrawLiveControls(LiveControlsUiState& state);
bool GetOverlayProjTans(const ImVec2& displaySize, float* tanHalfX, float* tanHalfY);
bool ProjectHandLocalPoint(const OpenXRHeadPose& headPose, const OpenXRHeadPose& handPose, float localX, float localY, float localZ, const ImVec2& displaySize, ImVec2* outScreen);
bool ProjectHeadSpacePointToScreen(float pointX, float pointY, float pointZ, const ImVec2& displaySize, ImVec2* outScreen);
bool ProjectIm3dPointToScreen(const Im3d::Vec3& point, const ImVec2& displaySize, ImVec2* outScreen);
extern bool g_drawAimRay;
extern bool g_drawBarrelCross;
extern bool g_drawHandDebugAxes;
extern bool g_drawHandLocator;
extern bool g_drawHandProxy3D;
extern float g_aimRayLenM;
extern float g_handLocatorScale;
void DrawBarrelCrosshair();
void DrawCompactAdsCameraTelemetry();
extern bool g_showCompactAdsTelemetry;
extern float g_compactAdsTelemetryX;
extern float g_compactAdsTelemetryY;
void DrawHandLocatorOverlay();
void DrawProjectedBone(ImDrawList* drawList, const OpenXRHeadPose& headPose, const OpenXRHeadPose& handPose, float ax, float ay, float az, float bx, float by, float bz, const ImVec2& displaySize, ImU32 color, float thickness);
void EmitHandProxyIm3d(const OpenXRHeadPose& handPose, bool isLeftHand, float scale, bool drawAxes);
void ReleaseGameMouseCapture();
void RenderIm3dToDrawList(ImDrawList* drawList, const ImVec2& displaySize);
void RotateVectorByQuaternion(float vx, float vy, float vz, float qx, float qy, float qz, float qw, float* outX, float* outY, float* outZ);
void UpdateImGuiMouseFromCursor(HWND hwnd, float backbufferWidth, float backbufferHeight);

// PRESENT-PACING STATISTICS.
//
// A mean frame rate cannot show judder, which is why "89-90 FPS but it feels choppy" is a coherent
// report rather than a contradiction: frames alternating 8 ms / 16 ms average to ~90 and look
// terrible. Two further things make the headline number untrustworthy here -- Present counts DLSS
// generated frames, and this port re-submits the last snapshot on display frames the game did not
// fill, so an external tool like fpsVR reads the full display rate regardless of the game's true
// cadence. So keep every frame's delta and report the distribution.
struct PerfStats {
    float presentFps;    // presents per second, averaged over the window
    float xrHz;          // XR cycles per second
    float vrcamFps;      // stereo camera copies per second
    float medianMs;
    float p99Ms;         // the "1% low": the frame time 99% of frames beat
    float maxMs;
    float stutterPct;    // share of frames longer than 1.5x the median
    float cadenceRatio;  // display refreshes per presented frame; whole numbers present evenly
    const float* history;
    unsigned historyCount;
};
void SamplePresentTiming();          // called once per present, from OverlayRender
void GetPerfStats(PerfStats* out);
void DrawPerformancePanel();

}  // namespace overlay
