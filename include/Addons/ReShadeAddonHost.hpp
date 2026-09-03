// A ReShade addon HOST, so a .addon64 can run without ReShade being in the process.
//
// WHY THIS EXISTS. renodx-dlss5.addon64 ("DLSS 5 Neural Rendering") drives nvngx_dlssnr.dll --
// a different NGX feature from the nvngx_dlssd.dll ray reconstruction the game itself uses --
// and exposes NR preset, HDR transfer strength and depth-inversion controls the game does not.
// It is the only thing in this process that knows how to drive that component. Reaching it
// through ReShade is not available here: ReShade 6.8 breakpoints in dxgi.dll about a second
// into a Cyberpunk launch (five for five), and 6.3.3, which is stable, predates the addon API
// version the addon asks for.
//
// So the plugin hosts it directly. An addon does not link against ReShade -- it calls
// K32EnumProcessModules, finds the first module exporting ReShadeRegisterAddon, and binds to
// that. Exporting the same ten entry points is therefore sufficient to be that module.
//
// EXPORTING THEM IS NOT FREE, and that is what the forwarding mode below is for. The addon's
// scan takes the FIRST module that exports the symbol and does not fall back if it refuses, so
// a plugin that exported these and declined would break any addon in a process where ReShade
// was working perfectly well. When hosting is off and a real ReShade is loaded, every call is
// forwarded to it instead, and the addon cannot tell the difference.

#pragma once

// One discovered setting. The addon's key names are not documented anywhere and are not
// recoverable from its binary -- it resolves everything by GetProcAddress and keeps the strings
// inline -- so the store is populated by watching what it ASKS FOR. `askedByAddon` marks a key
// it read (so the name is real), `setByAddon` marks one it wrote back.
struct CyberpunkVRAddonEntry {
    char section[64];
    char key[96];
    char value[192];
    int  askedByAddon;
    int  setByAddon;
};

struct CyberpunkVRAddonHostStatus {
    int      enabled;            // hosting on; 0 = forward to ReShade if present, else refuse
    int      forwarding;         // a real ReShade was found and is being forwarded to
    int      addonsFound;        // .addon64 files seen in the scan directory
    int      addonsLoaded;       // LoadLibraryW succeeded
    int      addonsRegistered;   // called ReShadeRegisterAddon and we accepted
    unsigned apiVersion;         // the version the last addon asked for (18 for renodx-dlss5)
    char     addonName[128];
    char     addonDescription[256];
    char     scanDir[260];
    char     lastError[256];
    // Which reshade::api::device vtable slots the addon called when init_device was delivered,
    // and the ImGui version it wanted a function table for. Both are measurements: they say how
    // much of ReShade's interface a real implementation would have to cover.
    char     vtCalls[192];
    unsigned imguiVersionAsked;
    int      initDeviceDelivered;  // the addon has been handed a device
    int      wantsPresent;         // it subscribed to event 74 (present)
    int      presentArmed;         // we are delivering that event
    int      presentFaulted;       // a delivery faulted; dispatch is off for this session
    unsigned long long presentCalls;
    int      presentPerView;       // VRCAM announced to the addon as its own swapchain
    int      hasOverlay;           // the addon registered a settings page
    int      drawOverlay;          // we are calling it
    int      overlayFaulted;
    char     imguiCalls[192];      // imgui_function_table slots its overlay used
    int      overlayStubReturn;    // what unimplemented stubs hand back to the page
    int      overlayWidgets;       // real forwarders installed for the measured controls
};

// Called once from the plugin's Load path. Reads reshade-addons.ini and, if hosting is enabled,
// starts the deferred load (see the .cpp -- the addon hooks NGX, so it must not be loaded before
// NGX is in the process).
extern "C" void CyberpunkVR_AddonHostInit();
// Called once per frame from the Present hook. Cheap and lock-free when nothing is armed.
extern "C" void CyberpunkVR_AddonHostOnPresent(void* swapChain);
// Calls the addon's registered settings page. Must be called from INSIDE an ImGui window.
// Draws the closed addon's real live master switch at the parent level. The ordinary overlay pass
// suppresses that same widget so users never see contradictory duplicate NR controls.
extern "C" int  CyberpunkVR_AddonHostDrawNeuralToggle(int* enabled);
extern "C" int  CyberpunkVR_AddonHostDrawOverlay();
extern "C" void CyberpunkVR_AddonHostSetDrawOverlay(int on);
extern "C" void CyberpunkVR_AddonHostSetStubReturn(int value);
extern "C" void CyberpunkVR_AddonHostSetOverlayWidgets(int on);
extern "C" void CyberpunkVR_AddonHostSetPresentPerView(int on);

// ---- consumed by the F10 overlay ---------------------------------------------------------------
extern "C" int  CyberpunkVR_AddonHostGetStatus(CyberpunkVRAddonHostStatus* out);
extern "C" void CyberpunkVR_AddonHostSetEnabled(int enabled);
extern "C" int  CyberpunkVR_AddonHostGetEntryCount();
extern "C" int  CyberpunkVR_AddonHostGetEntry(int index, CyberpunkVRAddonEntry* out);
extern "C" void CyberpunkVR_AddonHostSetEntry(const char* section, const char* key, const char* value);
// The events the addon subscribed to, by ReShade addon_event id. Empty means it asked for none,
// which is the answer to "does it need event delivery, or do its own NGX detours give it
// everything?" -- see the note on dispatch in the .cpp.
extern "C" int  CyberpunkVR_AddonHostGetEventCount();
extern "C" int  CyberpunkVR_AddonHostGetEvent(int index, unsigned* outEventId, int* outCallbacks);
