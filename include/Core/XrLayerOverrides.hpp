#pragma once

// Per-process OpenXR API-layer suppression.
//
// Implicit API layers are registered machine- or user-wide, so both switches the loader offers
// are global: delete the registry value, or set its DWORD non-zero. Either takes the layer away
// from EVERY OpenXR application, which is not an acceptable price for making one game work --
// ReShade has to keep post-processing every other title.
//
// The loader also honours each manifest's own `disable_environment` name, and it reads that from
// the environment of the process creating the instance. Setting those variables HERE disables the
// layers in this process and nowhere else. Nothing on the machine is modified, no other game is
// affected, and it reverts by deleting a line from vrport.ini.
//
// WHY THIS IS NEEDED AT ALL. ReShade's XR layer (XR_APILAYER_reshade) sits between us and
// SteamVR. On the first xrEndFrame the SteamVR runtime builds its D3D11-on-12 compositor interop,
// ReShade intercepts both that and the CreateDXGIFactory2 d3d11 makes internally, and dxgi.dll
// executes an int 3. Nothing handles a breakpoint with no debugger attached, so the process ends
// -- silently, with no WER report, no TDR and no dump, which is what made it so hard to find.
// ReShade's own log shows it then SKIPS the session ("without a proxy Direct3D 12 device"), so it
// was doing nothing for us while breaking the launch.
//
// Must run before xrCreateInstance. Called from the RED4ext Load, which is long before the
// swapchain exists, let alone the OpenXR instance.
void ApplyOpenXrLayerOverrides();
