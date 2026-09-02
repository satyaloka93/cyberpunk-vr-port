// See include/Addons/ReShadeAddonHost.hpp for why the plugin hosts ReShade addons at all.
//
// WHAT THE HOST HAS TO GET RIGHT, in the order the addon exercises it:
//
//  1. ReShadeRegisterAddon(module, api_version) -> bool. The gate. renodx-dlss5 asks for 18.
//  2. ReShadeGetImGuiFunctionTable(imgui_version) -> const void*. NOT optional, and returning
//     null is NOT the safe choice: reshade.hpp's register_addon returns false when the table
//     comes back null, so the addon aborts before doing anything. It is answered with a table
//     of no-op stubs, which is sound only because the overlay callback is never invoked -- see
//     ReShadeRegisterOverlay.
//  3. ReShadeGetConfigValue / SetConfigValue. Their API-18 ABI includes BOTH addon module and
//     effect-runtime arguments before section/key. Omitting the module shifts every argument:
//     `RenoDX.DLSS5` is mistaken for a key and `NRPreset` for its value, so no real setting can
//     persist. Keep these signatures byte-for-byte aligned with ReShade's reshade.hpp.
//  4. ReShadeRegisterEvent. Subscriptions are recorded, then the two events this addon needs are
//     dispatched by the host (init_device and present).
//
// The event and config traffic is logged on first sight, because the open question this build
// answers is what the addon actually needs from a host, and the log is the answer.

#include <windows.h>
#include <d3d12.h>
#include <imgui.h>
#include <utility>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <share.h>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

extern void Log(const char* fmt, ...);
extern char g_liveControlPath[MAX_PATH];   // vrport.ini; our ini is its sibling
extern "C" ID3D12Device* CyberpunkVR_GetGameDevice();
extern "C" ID3D12CommandQueue* CyberpunkVR_GetGameQueue();
extern "C" uint64_t CyberpunkVR_VrcamCtxKey();

#include "Addons/ReShadeAddonHost.hpp"

namespace {

// RECURSIVE, and it has to be. The addon binds to us from inside its own DllMain, which runs on
// the thread that is still inside our LoadLibrary call -- so ReShadeRegisterAddon re-enters this
// lock on a thread that already holds it. With a plain std::mutex MSVC throws system_error out of
// the re-lock, the exception unwinds through the addon's DllMain, and LoadLibrary reports
// ERROR_DLL_INIT_FAILED (1114) with no sign of why. Re-entry here is legitimate, not a bug to be
// designed away, so the lock says so.
std::recursive_mutex g_mtx;

struct Entry {
    std::string section, key, value;
    bool asked = false;
    bool set   = false;
};
std::vector<Entry> g_entries;

struct EventSub { unsigned id = 0; int callbacks = 0; std::vector<void*> fns; };
std::vector<EventSub> g_events;

bool  g_enabled     = false;
bool  g_started     = false;
bool  g_dispatchEvents = true;    // [host] dispatch_events -- the escape hatch if a fake object bites
unsigned g_imguiVersionAsked = 0; // what IMGUI_VERSION_NUM the addon wants a table for
char  g_vtCalls[192] = {};        // which api::device vtable slots the addon actually used
bool  g_initDeviceDelivered = false;
bool  g_dispatchPresent = true;   // [host] dispatch_present
// [host] present_per_view. Default 0 = one present per frame, which is what we have always sent.
//
// WHY THIS EXISTS. The addon's own log creates the model per view and the codec exactly once:
//     feature 18 created ..... 7      (MAIN and VRCAM, per configuration)
//     codec created .......... 1
//     inline NR resources .... 1
// RenoDX keys state per swapchain and per device (src/utils/swapchain.hpp, device.hpp). Its NGX
// detours fire twice a frame because the GAME drives them, but everything it keys per swapchain
// exists once, because this host reports exactly one device, one queue, one swapchain and one
// present. The codec is the per-swapchain half -- which is precisely the stage that is visible,
// and precisely the eye asymmetry being chased.
//
// 1 presents each view as its OWN swapchain object, so per-swapchain state is instantiated twice.
// This is inference about a closed binary: the addon may equally create a second codec AND a
// second inline resource set, on a path already at ~42 FPS.
int   g_presentPerView = 0;
bool  g_drawOverlay = true;       // [host] draw_overlay -- expose the addon's live settings page
// What the stub table returns. Widget calls that return a bool steer the page's own control flow,
// so 0 hides whatever sits behind a TreeNode/CollapsingHeader and 1 reveals it -- at the cost of
// the page running branches it would not normally run. Both readings are useful; neither is safe
// to assume, so it is a knob rather than a decision.
int   g_overlayStubReturn = 0;    // [host] overlay_stub_returns
// 0 = record argument shapes only and touch nothing. 1 = use the real forwarders below.
//
// WHAT ACTUALLY CAUSED THE THRASH, because the first explanation was wrong. It was not a
// mis-signed Checkbox scribbling memory -- it was overlay_stub_returns=1. A widget returning true
// means "the user changed this", so every UNIMPLEMENTED slot claimed a change on every frame, the
// addon committed whatever was in its stack local, and slot 80 (the unlabelled one, which the
// native/upscaling flip-flop tracks) rebuilt the NR feature 11 times against 2 evaluations.
//
// So the two knobs are mutually exclusive by construction: a stub that answers "true" is only ever
// safe while nothing real is wired, and turning widgets on forces stub returns back to 0.
int   g_overlayWidgets = 1;       // [host] overlay_widgets -- exact API-18 slots, safe default
bool  g_iniDirty = false;
DWORD g_lastIniWrite = 0;
void* g_overlayCallback = nullptr;
bool  g_overlayFaulted = false;
char  g_imguiCalls[192] = {};
std::atomic<bool> g_presentArmed{false};  // device delivered and a present callback exists
std::atomic<uint64_t> g_presentCalls{0};
std::atomic<bool> g_presentFaulted{false};
char  g_iniPath[MAX_PATH]   = {};
char  g_scanDir[MAX_PATH]   = {};
char  g_lastError[256]      = {};
int   g_addonsFound = 0, g_addonsLoaded = 0, g_addonsRegistered = 0;
unsigned g_apiVersion = 0;
char  g_addonName[128] = {};
char  g_addonDesc[256] = {};

// ---- forwarding --------------------------------------------------------------------------------
// Exporting the ReShade entry points makes this DLL a candidate host for EVERY addon in the
// process, including ones that were happily using a real ReShade. reshade.hpp binds to the first
// module that exports ReShadeRegisterAddon and does not try another if it refuses, so refusing is
// not a neutral act. When hosting is off we become a pass-through instead.
HMODULE g_forwardTo = nullptr;
bool    g_forwardResolved = false;

HMODULE FindOtherReShadeHost() {
    using PFN_Enum = BOOL(WINAPI*)(HANDLE, HMODULE*, DWORD, LPDWORD);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto enumMods = k32 ? reinterpret_cast<PFN_Enum>(GetProcAddress(k32, "K32EnumProcessModules")) : nullptr;
    if (!enumMods) return nullptr;

    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&FindOtherReShadeHost), &self);

    HMODULE mods[1024] = {};
    DWORD needed = 0;
    if (!enumMods(GetCurrentProcess(), mods, sizeof(mods), &needed)) return nullptr;
    const DWORD count = needed / sizeof(HMODULE);
    for (DWORD i = 0; i < count && i < 1024; ++i) {
        if (mods[i] == self) continue;
        if (GetProcAddress(mods[i], "ReShadeRegisterAddon") != nullptr) return mods[i];
    }
    return nullptr;
}

HMODULE ForwardTarget() {
    if (!g_forwardResolved) {
        g_forwardTo = FindOtherReShadeHost();
        g_forwardResolved = true;
        if (g_forwardTo) {
            wchar_t path[MAX_PATH] = {};
            GetModuleFileNameW(g_forwardTo, path, MAX_PATH);
            Log("[addonhost] hosting is off and a real ReShade host is present; forwarding to %ls\n", path);
        }
    }
    return g_forwardTo;
}

template <typename Fn>
Fn Forwarded(const char* name) {
    HMODULE m = ForwardTarget();
    return m ? reinterpret_cast<Fn>(GetProcAddress(m, name)) : nullptr;
}

// ---- safe string probing -----------------------------------------------------------------------
// ReShadeLogMessage's exact arity moved between ReShade generations, and getting it wrong means
// printing a register as a pointer. Rather than guess, each candidate is probed for being a
// readable NUL-terminated ASCII string and the first plausible one wins.
bool ReadableString(const void* p, size_t maxLen) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if ((mbi.Protect & readable) == 0) return false;
    if (mbi.Protect & PAGE_GUARD) return false;

    const auto* s = static_cast<const char*>(p);
    const auto* end = static_cast<const char*>(mbi.BaseAddress) + mbi.RegionSize;
    for (size_t i = 0; i < maxLen && (s + i) < end; ++i) {
        if (s[i] == '\0') return i > 0;
        // PRINTABLE ASCII ONLY. The looser test let an uninitialised section pointer through and
        // it was written into the ini as a section header of raw bytes.
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c != '\t' && (c < 0x20 || c > 0x7E)) return false;
    }
    return false;
}

void CopyBounded(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    dst[0] = '\0';
    if (!src) return;
    strncpy_s(dst, cap, src, _TRUNCATE);
}

// ---- the ini -----------------------------------------------------------------------------------
void ResolveIniPath() {
    if (g_iniPath[0]) return;
    CopyBounded(g_iniPath, MAX_PATH, g_liveControlPath);
    char* slash = strrchr(g_iniPath, '\\');
    if (slash) { slash[1] = '\0'; strncat_s(g_iniPath, MAX_PATH, "reshade-addons.ini", _TRUNCATE); }
    else       { CopyBounded(g_iniPath, MAX_PATH, "reshade-addons.ini"); }
}

void ResolveScanDir() {
    if (g_scanDir[0]) return;
    // The game exe's folder -- bin\x64 -- because that is where ReShade would have looked and so
    // that is where the .addon64 already is. Our own DLL lives under red4ext\plugins\.
    GetModuleFileNameA(nullptr, g_scanDir, MAX_PATH);
    char* slash = strrchr(g_scanDir, '\\');
    if (slash) slash[1] = '\0';
}

Entry* FindEntry(const char* section, const char* key) {
    for (auto& e : g_entries)
        if (_stricmp(e.section.c_str(), section ? section : "") == 0 &&
            _stricmp(e.key.c_str(), key ? key : "") == 0)
            return &e;
    return nullptr;
}

Entry& TouchEntry(const char* section, const char* key) {
    if (Entry* e = FindEntry(section, key)) return *e;
    g_entries.push_back(Entry{section ? section : "", key ? key : "", "", false, false});
    return g_entries.back();
}

void LoadIni() {
    ResolveIniPath();
    FILE* f = _fsopen(g_iniPath, "r", _SH_DENYNO);
    if (!f) return;
    char line[512];
    std::string section = "host";
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        char* nl = strpbrk(p, "\r\n");
        if (nl) *nl = '\0';
        if (!*p || *p == ';' || *p == '#') continue;
        if (*p == '[') {
            char* close = strchr(p, ']');
            if (close) { *close = '\0'; section = p + 1; }
            continue;
        }
        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = p;
        char* val = eq + 1;
        for (char* t = key + strlen(key); t > key && (t[-1] == ' ' || t[-1] == '\t'); --t) t[-1] = '\0';
        while (*val == ' ' || *val == '\t') ++val;

        if (_stricmp(section.c_str(), "host") == 0) {
            if (_stricmp(key, "enabled") == 0) g_enabled = (atoi(val) != 0);
            else if (_stricmp(key, "dispatch_events") == 0) g_dispatchEvents = (atoi(val) != 0);
            else if (_stricmp(key, "dispatch_present") == 0) g_dispatchPresent = (atoi(val) != 0);
            else if (_stricmp(key, "present_per_view") == 0) g_presentPerView = atoi(val);
            else if (_stricmp(key, "draw_overlay") == 0) g_drawOverlay = (atoi(val) != 0);
            else if (_stricmp(key, "overlay_stub_returns") == 0) g_overlayStubReturn = atoi(val);
            else if (_stricmp(key, "overlay_widgets") == 0) g_overlayWidgets = atoi(val);
            else if (_stricmp(key, "addon_dir") == 0 && *val) CopyBounded(g_scanDir, MAX_PATH, val);
            continue;
        }
        Entry& e = TouchEntry(section.c_str(), key);
        e.value = val;
    }
    fclose(f);
}

void SaveIniLocked() {
    ResolveIniPath();
    FILE* f = _fsopen(g_iniPath, "w", _SH_DENYNO);
    if (!f) return;
    fprintf(f, "; Written by CyberpunkVR_Stereo's ReShade addon host.\n");
    fprintf(f, "; enabled=1 makes this plugin host bin\\x64\\*.addon64 itself, with no ReShade.\n");
    fprintf(f, "; Keys below appear as the addon asks for them; edit them here or in the F10 menu.\n");
    fprintf(f, "[host]\nenabled=%d\ndispatch_events=%d\ndispatch_present=%d\npresent_per_view=%d\ndraw_overlay=%d\noverlay_stub_returns=%d\noverlay_widgets=%d\n",
            g_enabled ? 1 : 0, g_dispatchEvents ? 1 : 0, g_dispatchPresent ? 1 : 0, g_presentPerView,
            g_drawOverlay ? 1 : 0, g_overlayStubReturn, g_overlayWidgets);
    std::string current;
    for (const auto& e : g_entries) {
        if (e.section != current) { current = e.section; fprintf(f, "\n[%s]\n", current.c_str()); }
        fprintf(f, "%s=%s\n", e.key.c_str(), e.value.c_str());
    }
    fclose(f);
}

// ---- the ImGui function table ------------------------------------------------------------------
// A block of identical no-op stubs. It exists to satisfy the null check in reshade.hpp, nothing
// more: the addon copies the whole struct out of this buffer, so the buffer only has to be larger
// than the struct, and every entry only has to be a valid code address. It would be reached solely
// from the overlay callback, which is never called.
// ---- the seven slots the settings page actually uses -------------------------------------------
// Discovered by logging each stub's first argument, which for an ImGui widget is its label:
//
//   115  "Enable DLSS Neural Rendering"              checkbox
//   103  "Control-compatible color transfer"         checkbox
//   129  "NR Preset"                                 combo
//   144  "NR Intensity"                              slider
//   111  "Reset NR feature and clear failure latch"  button
//   104  "DLSSNR v310.8.0: %s"                       text (a format string, so variadic)
//    80  (no label)                                  left as a recording stub
//
// The labels are load-bearing here. Counting members of ReShade's imgui_function_table put 115 at
// ColorEdit3 and 129 at TreePop -- which no settings page would call in that order -- so the
// arithmetic was wrong and the labels are what these are built from instead.

const char* Dlss5ControlHelp(const char* label) {
    if (!label) return nullptr;
    struct Help { const char* label; const char* text; };
    static const Help help[] = {
        {"Enable DLSS Neural Rendering",
         "Master switch for feature 18. OFF leaves Cyberpunk's ordinary DLSS Super Resolution untouched."},
        {"Enable Upscaling",
         "Do not toggle this during VR testing. In this addon/runtime it feeds NR at the low guide resolution, then enlarges the populated region; it caused shimmer and the latest progressive 18 -> 11 FPS stall. Keep OFF for native NR after ordinary DLSS SR."},
        {"NR Preset",
         "Opaque DLSSNR.Hint.Render.Preset selector: Default or Preset #1-#3. The closed addon does not document which model/tuning each number selects. It is separate from Natural/Cinematic and changing it rebuilds both eye features."},
        {"NR Style",
         "Model style selector: Default, Natural, or Cinematic. The exact training/tuning difference is not public. It is separate from Preset; compare it on one stationary scene because it can reinterpret lighting and materials."},
        {"NR Intensity",
         "Strength passed to DLSSNR.Intensity. Higher values ask for a stronger neural change/relighting; 1.0 is the known neutral starting point. This may magnify binocular lighting differences."},
        {"Local Tone Strength",
         "Strength passed to DLSSNR.LocalToneStrength. Controls local tonal/lighting adaptation rather than geometric detail. Exact model semantics are undocumented; 1.0 is the addon's normal starting point."},
        {"Local Structure Strength",
         "Strength passed to DLSSNR.LocalStructureStrength. Controls local structure/detail influence, including edges and reflective texture. Higher values may make independently generated eye detail disagree more."},
        {"Skin Structure Strength",
         "Strength passed to DLSSNR.SkinStructureStrength. Targets the model's skin-detail channel. -1 asks for the addon/model default; positive values explicitly strengthen it. Test on one close face."},
        {"Automatic Mask",
         "Passes DLSSNR.UseAutoMask. Lets the model derive its own region mask instead of relying only on uniform strengths. The closed binary does not document exactly which subjects the mask selects."},
        {"NR UI Correction",
         "Passes DLSSNR.UICorrection. Intended to reduce Neural Rendering changes on UI/HUD elements that are present in the processed color image."},
        {"Depth Convention",
         "How depth is interpreted: game/default, force inverted, or force normal. A wrong choice corrupts temporal guides. Leave at Default unless a depth diagnostic proves inversion is wrong."},
        {"Motion Scale X Multiplier",
         "Extra multiplier on horizontal motion vectors before DLSSNR. Leave at 1.0. Sign/scale changes are diagnostics for doubling, smearing, or motion ghosting, not quality presets."},
        {"Motion Scale Y Multiplier",
         "Extra multiplier on vertical motion vectors before DLSSNR. Leave at 1.0. Sign/scale changes are diagnostics for doubling, smearing, or motion ghosting, not quality presets."},
        {"Scene Paper-White Scale",
         "Normalization used by the addon's Control-compatible HDR transfer codec. It is not a face-detail control. Large changes alter scene brightness/color handling; keep near the addon's default unless HDR paper white is being calibrated."},
        {"HDR Transfer Strength",
         "Closed-addon codec control. A neutral-native A/B showed it changes MAIN/left but not VRCAM/right, so any non-zero value grades one eye only. 0 is the stereo-safe value; the independent Native ReShade color pass is the way to get binocular tone."},
        {"Color Strength",
         "Closed-addon codec control. A neutral-native A/B showed it changes MAIN/left but not VRCAM/right, so any non-zero value grades one eye only. 0 is the stereo-safe value; the independent Native ReShade color pass is the way to get binocular color."},
        {"Reset NR feature and clear failure latch",
         "Releases and recreates the NR feature after a latched failure. Use once only when status says STANDBY/FAILED; repeated resets or setting churn can stall the renderer."},
        {"Control-compatible color transfer",
         "Post-model codec that maps the neural result through a Control-style HDR/tonemap transfer. These controls can change brightness/color without proving the model changed geometry or faces."},
        {"Guide overrides (leave at defaults unless diagnostics require them)",
         "Depth and motion-vector convention overrides. Leave them at default/1.0 unless a captured guide diagnostic identifies a sign, scale, or inversion error."},
    };
    for (const auto& h : help) if (strcmp(label, h.label) == 0) return h.text;
    return nullptr;
}

void DrawDlss5ControlHelp(const char* label) {
    if (!ImGui::IsItemHovered()) return;

    // SetTooltip() creates an auto-sized window, so these paragraph-length explanations expand
    // into a single line wider than the VR overlay. Give every DLSS5 tooltip a scale-aware reading
    // measure instead. Cursor-relative wrap position is required by ImGui: PushTextWrapPos expects
    // a local window X coordinate, not a width.
    constexpr float kTooltipWidthInEms = 32.0f;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kTooltipWidthInEms);
    if (const char* text = Dlss5ControlHelp(label)) {
        ImGui::TextUnformatted(text);
    } else {
        ImGui::Text("Closed-addon control '%s'; its exact semantics are not documented.", label);
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

bool ImplCheckbox(const char* label, bool* v) {
    if (!ReadableString(label, 128) || !v) return false;
    if (strcmp(label, "Enable Upscaling") == 0) {
        // A live native->upscaling->native transition created three feature generations and then
        // degraded from ~50 FPS to 18, 14 and finally 11.5 FPS while XR kept submitting. Keep the
        // raw next-launch key for deliberate diagnostics, but do not let an innocent F10 click
        // repeat that renderer-stall path.
        bool shown = *v;
        ImGui::BeginDisabled();
        ImGui::Checkbox(label, &shown);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("locked off in VR");
        DrawDlss5ControlHelp(label);
        return false;
    }
    const bool changed = ImGui::Checkbox(label, v);
    DrawDlss5ControlHelp(label);
    return changed;
}

bool ImplButton(const char* label, const ImVec2& size) {
    if (!ReadableString(label, 128)) return false;
    const bool changed = ImGui::Button(label, size);
    DrawDlss5ControlHelp(label);
    return changed;
}

bool ImplSliderFloat(const char* label, float* v, float vmin, float vmax,
                     const char* fmt, int flags) {
    if (!ReadableString(label, 128) || !v) return false;
    const char* f = ReadableString(fmt, 32) ? fmt : "%.3f";
    if (!(vmin < vmax)) { vmin = 0.0f; vmax = 1.0f; }   // junk range = unusable widget
    const bool changed = ImGui::SliderFloat(label, v, vmin, vmax, f, flags);
    DrawDlss5ControlHelp(label);
    return changed;
}

// These signatures and slots come from ReShade's actual API-18
// imgui_function_table_19250.hpp (the exact version requested by this addon), not inference from
// stack values. In particular slot 80 is void Separator(), not an "Enable Upscaling" control;
// slot 103 is TextUnformatted(), not a checkbox. Every setting of a given widget type flows through
// the same slot, so one correct Checkbox forwarder exposes Enable NR, Enable Upscaling and AutoMask.
bool ImplCombo(const char* label, int* current, const char* const items[], int count, int popupMax) {
    if (!ReadableString(label, 128) || !current || !items || count <= 0 || count >= 256) return false;
    const bool changed = ImGui::Combo(label, current, items, count, popupMax);
    DrawDlss5ControlHelp(label);
    return changed;
}

void ImplSeparator() {
    ImGui::Separator();
}

void ImplTextUnformatted(const char* text, const char* textEnd) {
    if (!ReadableString(text, 512)) return;
    // The current addon passes null for textEnd. Ignore an untrusted non-null end pointer rather
    // than walking it; every label/status string in this page is NUL terminated.
    (void)textEnd;
    ImGui::TextUnformatted(text);
    if (Dlss5ControlHelp(text)) DrawDlss5ControlHelp(text);
}

void ImplTextV(const char* fmt, va_list args) {
    if (!ReadableString(fmt, 256)) return;
    ImGui::TextV(fmt, args);
}

constexpr int kImGuiSlots = 512;   // logged individually; the rest stay shared no-ops
void* g_imguiTable[8192];
bool  g_imguiSeen[kImGuiSlots] = {};

void NoteImGuiCall(int slot) {
    if (slot < 0 || slot >= kImGuiSlots || g_imguiSeen[slot]) return;
    g_imguiSeen[slot] = true;
    Log("[addonhost] addon's overlay called imgui_function_table slot %d.\n", slot);
    const size_t used = strlen(g_imguiCalls);
    char tmp[16];
    _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s%d", used ? "," : "", slot);
    if (used + strlen(tmp) + 1 < sizeof(g_imguiCalls))
        strncat_s(g_imguiCalls, sizeof(g_imguiCalls), tmp, _TRUNCATE);
}

void ImGuiStub() {}

// THE LABEL IS THE ANSWER. Mapping a slot index onto a member of ReShade's table means trusting a
// count over ~400 entries against a header whose layout is version-gated -- and the first attempt
// produced a set (DragInt, ColorEdit3, TreePop with no TreeNode) that does not describe a settings
// page. Nearly every ImGui widget takes `const char *label` first, so the argument says what the
// control IS regardless of whether the index arithmetic is right.
// Records the SHAPE of the call, which is what the guessed forwarders got wrong. The label says
// what the control is; dereferencing the second argument says what type the addon keeps it in --
// a bool reads 0 or 1, a float reads something in a plausible range, an int reads a small index.
// That is enough to write a forwarder that does not have to assume.
bool DerefPreview(void* p, char* out, size_t cap) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT) return false;
    const DWORD ok = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                     PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if ((mbi.Protect & ok) == 0 || (mbi.Protect & PAGE_GUARD)) return false;
    uint32_t raw = 0;
    memcpy(&raw, p, sizeof(raw));
    float f = 0.0f;
    memcpy(&f, &raw, sizeof(f));
    _snprintf_s(out, cap, _TRUNCATE, "u32=%u i32=%d f32=%.4f byte0=%u",
                raw, static_cast<int>(raw), f, raw & 0xFFu);
    return true;
}

template <int I> uint64_t ImGuiSlotStub(void* a1, void* a2, void* a3, void* a4) {
    (void)a4;
    if (!g_imguiSeen[I < kImGuiSlots ? I : 0]) {
        const char* label = ReadableString(a1, 96) ? static_cast<const char*>(a1)
                          : ReadableString(a2, 96) ? static_cast<const char*>(a2) : nullptr;
        char deref[96] = {};
        const bool haveDeref = DerefPreview(a2, deref, sizeof(deref));
        Log("[addonhost]   slot %d label \"%s\" arg2=%p%s%s arg3=%s\n",
            I, label ? label : "(none)", a2,
            haveDeref ? " -> " : "", haveDeref ? deref : "",
            ReadableString(a3, 64) ? static_cast<const char*>(a3) : "(not a string)");
    }
    NoteImGuiCall(I);
    return static_cast<uint64_t>(g_overlayStubReturn);
}
template <int... I> void FillImGuiTable(std::integer_sequence<int, I...>) {
    ((g_imguiTable[I] = reinterpret_cast<void*>(&ImGuiSlotStub<I>)), ...);
}

void BuildImGuiTable() {
    for (auto& slot : g_imguiTable) slot = reinterpret_cast<void*>(&ImGuiStub);
    // Only the first kImGuiSlots are individually identifiable. That is the whole point of this
    // build: an addon settings page is a few dozen ImGui calls, and the slot numbers say WHICH,
    // so only those have to be implemented for real against our own ImGui.
    FillImGuiTable(std::make_integer_sequence<int, kImGuiSlots>{});
    // OPT-IN. Recording is the default because these signatures are inferred, and an inferred
    // signature that writes through a pointer corrupts the addon rather than merely failing.
    if (!g_overlayWidgets) return;
    // Exact ReShade 1.92.5 table positions used by this addon. These cover the whole current page,
    // not just the first label observed at each slot: all checkboxes share 115, all array combos
    // share 129, and all float sliders share 144.
    g_imguiTable[80]  = reinterpret_cast<void*>(&ImplSeparator);
    g_imguiTable[103] = reinterpret_cast<void*>(&ImplTextUnformatted);
    g_imguiTable[104] = reinterpret_cast<void*>(&ImplTextV);
    g_imguiTable[111] = reinterpret_cast<void*>(&ImplButton);
    g_imguiTable[115] = reinterpret_cast<void*>(&ImplCheckbox);
    g_imguiTable[129] = reinterpret_cast<void*>(&ImplCombo);
    g_imguiTable[144] = reinterpret_cast<void*>(&ImplSliderFloat);
}


// ---- the api::device the addon is waiting for --------------------------------------------------
// The addon subscribes to init_device (event 0) and does nothing until it arrives. ReShade would
// hand it a reshade::api::device -- an abstract class whose vtable is ReShade's own D3D12 wrapper,
// which does not exist here and would be a port to reproduce.
//
// What DOES exist is the question "how much of that interface does this addon actually touch?".
// So it gets an object whose vtable is 128 distinct thunks, each of which records the slot it was
// called through. api_object::get_native() -- the handle to the real ID3D12Device -- is the first
// virtual and the one an addon that hooks NGX itself would plausibly be after, so every slot
// returns the real device: right for get_native, harmless for a slot that is only being counted.
//
// This is a measurement, not an implementation, and it is behind dispatch_events for that reason.
// The slot list it produces is what says whether a real implementation is a morning's work or a
// port of ReShade's device wrapper.
constexpr int kVtSlots = 128;
void* g_fakeDeviceVt[kVtSlots];
void* g_fakeDevice[2] = { g_fakeDeviceVt, nullptr };   // [0] = vptr, [1] = spare word
uint64_t g_nativeDevice = 0;
bool g_vtSeen[kVtSlots] = {};

void NoteVtCall(int slot) {
    if (slot < 0 || slot >= kVtSlots || g_vtSeen[slot]) return;
    g_vtSeen[slot] = true;
    Log("[addonhost] addon called api::device vtable slot %d.\n", slot);
    const size_t used = strlen(g_vtCalls);
    char tmp[16];
    _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s%d", used ? "," : "", slot);
    if (used + strlen(tmp) + 1 < sizeof(g_vtCalls)) strncat_s(g_vtCalls, sizeof(g_vtCalls), tmp, _TRUNCATE);
}

template <int I>
uint64_t __fastcall FakeVtSlot(void*) {
    NoteVtCall(I);
    return g_nativeDevice;
}

template <int... I>
void FillFakeVt(std::integer_sequence<int, I...>) {
    ((g_fakeDeviceVt[I] = reinterpret_cast<void*>(&FakeVtSlot<I>)), ...);
}


// ---- the per-frame present callback ------------------------------------------------------------
// Event 74 is `present`, and it is where this addon does everything: init_device only cached the
// pointer, and the NGX hooks it advertises are not installed until a frame goes by. Its signature
// is
//     void (api::command_queue*, api::swapchain*, const int32_t *source_rect,
//           const int32_t *dest_rect, uint32_t dirty_rect_count, const api::rect *dirty_rects)
// so it needs two more objects. They get the same measuring vtables as the device did, returning
// the real ID3D12CommandQueue / IDXGISwapChain from every slot -- correct for get_native(), and
// counted for anything else.
//
// THE SEH GUARD IS THE POINT OF THIS BEING SAFE TO SHIP. This runs on the present thread, every
// frame, calling into a third-party binary through an interface we are approximating. A fault
// here without the guard is a hard crash on every future frame; with it, the first fault disables
// present dispatch for the rest of the session and says so, and the last logged vtable slot names
// the method that has to be real.
void* g_fakeQueueVt[kVtSlots];
void* g_fakeSwapVt[kVtSlots];
void* g_fakeQueue[2] = { g_fakeQueueVt, nullptr };
void* g_fakeSwap[2]  = { g_fakeSwapVt,  nullptr };
// A DISTINCT OBJECT, not a second pointer to the same one: per-swapchain state is keyed by the
// api::swapchain* the callback receives, so the identity is the whole point. It shares the vtable
// because the addon has never called a single slot on any of these.
void* g_fakeSwapVrcam[2] = { g_fakeSwapVt, nullptr };
uint64_t g_nativeSwapVrcam = 0;
uint64_t g_nativeQueue = 0, g_nativeSwap = 0;
bool g_vtQueueSeen[kVtSlots] = {}, g_vtSwapSeen[kVtSlots] = {};

void NoteOtherVtCall(const char* what, bool* seen, int slot) {
    if (slot < 0 || slot >= kVtSlots || seen[slot]) return;
    seen[slot] = true;
    Log("[addonhost] addon called api::%s vtable slot %d.\n", what, slot);
}

template <int I> uint64_t __fastcall FakeQueueSlot(void*) {
    NoteOtherVtCall("command_queue", g_vtQueueSeen, I); return g_nativeQueue;
}
template <int I> uint64_t __fastcall FakeSwapSlot(void* self) {
    NoteOtherVtCall("swapchain", g_vtSwapSeen, I);
    return (self == g_fakeSwapVrcam) ? g_nativeSwapVrcam : g_nativeSwap;
}
template <int... I> void FillQueueVt(std::integer_sequence<int, I...>) {
    ((g_fakeQueueVt[I] = reinterpret_cast<void*>(&FakeQueueSlot<I>)), ...);
}
template <int... I> void FillSwapVt(std::integer_sequence<int, I...>) {
    ((g_fakeSwapVt[I] = reinterpret_cast<void*>(&FakeSwapSlot<I>)), ...);
}

using PresentFn = void (*)(void*, void*, const int32_t*, const int32_t*, uint32_t, const void*);

// Separate, object-free function: __try/__except cannot live in a frame that needs C++ unwinding.
bool InvokePresentGuarded(PresentFn fn, void* swapObject) {
    __try {
        fn(g_fakeQueue, swapObject, nullptr, nullptr, 0, nullptr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool InvokeDeviceEventGuarded(void* fn, void* deviceObject) {
    __try {
        reinterpret_cast<void(*)(void*)>(fn)(deviceObject);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void DispatchInitDevice() {
    if (!g_dispatchEvents) {
        Log("[addonhost] dispatch_events=0; init_device NOT delivered, so the addon will stay idle.\n");
        return;
    }
    ID3D12Device* dev = CyberpunkVR_GetGameDevice();
    if (!dev) {
        Log("[addonhost] no game device yet; init_device not delivered.\n");
        return;
    }
    g_nativeDevice = reinterpret_cast<uint64_t>(dev);
    FillFakeVt(std::make_integer_sequence<int, kVtSlots>{});

    for (const auto& e : g_events) {
        if (e.id != 0) continue;   // init_device
        for (void* fn : e.fns) {
            Log("[addonhost] delivering init_device(device=%p) to %p ...\n", dev, fn);
            reinterpret_cast<void(*)(void*)>(fn)(g_fakeDevice);
            Log("[addonhost] init_device returned.\n");
            g_initDeviceDelivered = true;
        }
    }
    if (!g_dispatchPresent) {
        Log("[addonhost] dispatch_present=0; the addon will stay idle.\n");
        return;
    }
    ID3D12CommandQueue* q = CyberpunkVR_GetGameQueue();
    g_nativeQueue = reinterpret_cast<uint64_t>(q);
    FillQueueVt(std::make_integer_sequence<int, kVtSlots>{});
    FillSwapVt(std::make_integer_sequence<int, kVtSlots>{});
    for (const auto& e : g_events) {
        if (e.id == 74 && !e.fns.empty()) {
            g_presentArmed.store(true, std::memory_order_release);
            Log("[addonhost] present dispatch armed (queue=%p, %zu callback(s), per-view=%d).\n",
                q, e.fns.size(), g_presentPerView);
        }
    }
}


// ---- the addon's own settings page -------------------------------------------------------------
// The addon keeps NR Preset, HDR Transfer Strength and the enables in an ImGui overlay, not in
// config -- so the only way to reach them is to call that overlay ourselves, from inside our own
// F10 window, with an imgui_function_table it accepts.
//
// It wants the table for IMGUI_VERSION_NUM 19250 and we bundle 19090, so a blind full
// implementation would be a lot of work against a layout we would be guessing at. Instead the
// table is stubs that record their slot, exactly as the device vtable was, and the slot numbers
// name the handful of functions a real implementation has to cover. Nothing is drawn on this pass.
void* g_fakeRuntimeVt[kVtSlots];
void* g_fakeRuntime[2] = { g_fakeRuntimeVt, nullptr };
bool  g_vtRuntimeSeen[kVtSlots] = {};

template <int I> uint64_t __fastcall FakeRuntimeSlot(void*) {
    NoteOtherVtCall("effect_runtime", g_vtRuntimeSeen, I); return 0;
}
template <int... I> void FillRuntimeVt(std::integer_sequence<int, I...>) {
    ((g_fakeRuntimeVt[I] = reinterpret_cast<void*>(&FakeRuntimeSlot<I>)), ...);
}

bool InvokeOverlayGuarded(void (*fn)(void*)) {
    __try { fn(g_fakeRuntime); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ---- loading -----------------------------------------------------------------------------------
void LoadAddonsNow() {
    ResolveScanDir();
    std::string pattern = std::string(g_scanDir) + "*.addon64";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        CopyBounded(g_lastError, sizeof(g_lastError), "no *.addon64 found in the game exe folder");
        Log("[addonhost] no *.addon64 in %s -- nothing to host.\n", g_scanDir);
        return;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        ++g_addonsFound;
        std::string full = std::string(g_scanDir) + fd.cFileName;
        // The addon binds to us from its own DllMain, so by the time LoadLibrary returns it has
        // either registered or refused, and g_addonsRegistered already says which.
        const int before = g_addonsRegistered;
        HMODULE m = LoadLibraryA(full.c_str());
        if (!m) {
            const DWORD err = GetLastError();
            _snprintf_s(g_lastError, sizeof(g_lastError), _TRUNCATE,
                        "LoadLibrary('%s') failed, error %lu", fd.cFileName, err);
            Log("[addonhost] LoadLibrary %s failed (error %lu)\n", fd.cFileName, err);
            continue;
        }
        ++g_addonsLoaded;
        Log("[addonhost] loaded %s -- %s\n", fd.cFileName,
            g_addonsRegistered > before ? "registered" : "did NOT register (see its own log lines)");
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// The addon installs Detours hooks on NGX ("D3D12 NGX hooks installed; inline DLSS contract
// capture armed"). Loading it before NGX is in the process would mean hooking nothing, so the
// load waits for _nvngx.dll. It proceeds anyway on timeout rather than silently never running --
// the addon's own log lines are more useful than our guess about why it did not start.
void DeferredLoadThread() {
    // TWO WAITS, NOT ONE, and the first attempt conflated them. NGX is in the process well before
    // the game creates its D3D12 device -- Streamline's interposer pulls it in early -- so waiting
    // on NGX alone reached LoadLibrary with CyberpunkVR_GetGameDevice() still null and delivered
    // no init_device at all. Loading still keys off NGX, because the addon's detours want to be in
    // place before the first DLSS create; delivery keys off the device, separately.
    const DWORD ngxDeadline = GetTickCount() + 90000;
    for (;;) {
        if (GetModuleHandleW(L"_nvngx.dll") || GetModuleHandleW(L"nvngx_dlssnr.dll")) {
            Log("[addonhost] NGX present; loading addons.\n");
            break;
        }
        if (GetTickCount() > ngxDeadline) {
            Log("[addonhost] NGX never appeared in 90 s; loading addons anyway.\n");
            break;
        }
        Sleep(250);
    }
    {
        std::lock_guard<std::recursive_mutex> lock(g_mtx);
        LoadAddonsNow();
        if (g_addonsRegistered == 0) return;   // nothing subscribed; nothing to deliver
    }

    const DWORD devDeadline = GetTickCount() + 180000;
    for (;;) {
        if (CyberpunkVR_GetGameDevice()) break;
        if (GetTickCount() > devDeadline) {
            std::lock_guard<std::recursive_mutex> lock(g_mtx);
            CopyBounded(g_lastError, sizeof(g_lastError),
                        "the game device never appeared, so init_device was never delivered");
            Log("[addonhost] no game device after 180 s; init_device never delivered.\n");
            return;
        }
        Sleep(250);
    }
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    DispatchInitDevice();
}

} // namespace

// ================================================================================================
// The ReShade addon entry points.
// ================================================================================================

extern "C" __declspec(dllexport)
bool ReShadeRegisterAddon(HMODULE addon_module, uint32_t api_version) {
    // Logged BEFORE the lock and before any decision: "the addon found us" and "we accepted it"
    // are different failures, and the last run could not tell them apart.
    Log("[addonhost] ReShadeRegisterAddon(module=%p, api=%u) entered.\n", addon_module, api_version);
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (!g_enabled) {
        if (auto fn = Forwarded<bool(*)(HMODULE, uint32_t)>("ReShadeRegisterAddon"))
            return fn(addon_module, api_version);
        return false;
    }
    g_apiVersion = api_version;

    // NAME and DESCRIPTION are the addon's only exports and are const char* GLOBALS, not
    // functions -- GetProcAddress hands back the address of the pointer variable.
    if (auto** name = reinterpret_cast<const char**>(GetProcAddress(addon_module, "NAME")))
        if (ReadableString(*name, 127)) CopyBounded(g_addonName, sizeof(g_addonName), *name);
    if (auto** desc = reinterpret_cast<const char**>(GetProcAddress(addon_module, "DESCRIPTION")))
        if (ReadableString(*desc, 255)) CopyBounded(g_addonDesc, sizeof(g_addonDesc), *desc);

    ++g_addonsRegistered;
    Log("[addonhost] registered addon \"%s\" (API version %u) -- %s\n",
        g_addonName[0] ? g_addonName : "<unnamed>", api_version,
        g_addonDesc[0] ? g_addonDesc : "<no description>");
    return true;
}

extern "C" __declspec(dllexport)
void ReShadeUnregisterAddon(HMODULE addon_module) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (!g_enabled) {
        if (auto fn = Forwarded<void(*)(HMODULE)>("ReShadeUnregisterAddon")) fn(addon_module);
        return;
    }
    (void)addon_module;
    if (g_addonsRegistered > 0) --g_addonsRegistered;
    Log("[addonhost] addon unregistered.\n");
}

// EVENTS ARE RECORDED, NOT DISPATCHED. Dispatching would mean handing the addon ReShade's own
// device/command-list/swapchain wrapper objects, which do not exist here -- a plausible-looking
// pointer would be worse than no call at all. The open question is whether this addon needs any
// of them or whether its NGX detours already give it the command list it evaluates on; the ids
// logged here answer that, and this is the build that collects them.
extern "C" __declspec(dllexport)
void ReShadeRegisterEvent(uint32_t ev, void* callback) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (!g_enabled) {
        if (auto fn = Forwarded<void(*)(uint32_t, void*)>("ReShadeRegisterEvent")) fn(ev, callback);
        return;
    }
    for (auto& e : g_events) {
        if (e.id == ev) { ++e.callbacks; e.fns.push_back(callback); return; }
    }
    g_events.push_back(EventSub{ev, 1, {callback}});
    Log("[addonhost] addon subscribed to event id %u (callback %p) -- recorded, not dispatched.\n",
        ev, callback);
}

extern "C" __declspec(dllexport)
void ReShadeUnregisterEvent(uint32_t ev, void* callback) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (!g_enabled) {
        if (auto fn = Forwarded<void(*)(uint32_t, void*)>("ReShadeUnregisterEvent")) fn(ev, callback);
        return;
    }
    for (auto& e : g_events)
        if (e.id == ev && e.callbacks > 0) { --e.callbacks; return; }
}

extern "C" __declspec(dllexport)
void ReShadeRegisterOverlay(const char* title, void* callback) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (!g_enabled) {
        if (auto fn = Forwarded<void(*)(const char*, void*)>("ReShadeRegisterOverlay")) fn(title, callback);
        return;
    }
    g_overlayCallback = callback;
    Log("[addonhost] addon registered overlay \"%s\" (%p) -- its settings page; draw_overlay "
        "controls whether the F10 menu calls it.\n",
        ReadableString(title, 128) ? title : "<untitled/settings page>", callback);
}

extern "C" __declspec(dllexport)
void ReShadeUnregisterOverlay(const char* title, void* callback) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (!g_enabled) {
        if (auto fn = Forwarded<void(*)(const char*, void*)>("ReShadeUnregisterOverlay")) fn(title, callback);
        return;
    }
    (void)title; (void)callback;
}

extern "C" __declspec(dllexport)
void ReShadeLogMessage(HMODULE module, uint32_t level, const char* message) {
    if (!g_enabled) {
        if (auto fn = Forwarded<void(*)(HMODULE, uint32_t, const char*)>("ReShadeLogMessage"))
            fn(module, level, message);
        return;
    }
    (void)level;
    // Arity has moved between ReShade generations, so take whichever argument is actually a
    // string. Everything the addon says about itself lands in cyberpunkvrport.log.
    const char* text = nullptr;
    if (ReadableString(message, 1024))                                     text = message;
    else if (ReadableString(reinterpret_cast<const char*>(module), 1024))  text = reinterpret_cast<const char*>(module);
    if (!text) return;
    const size_t n = strlen(text);
    Log("[addon] %s%s", text, (n && text[n - 1] == '\n') ? "" : "\n");
}

extern "C" __declspec(dllexport)
bool ReShadeGetConfigValue(void* module, void* runtime, const char* section, const char* key,
                           char* value, size_t* value_size) {
    if (!g_enabled) {
        if (auto fn = Forwarded<bool(*)(void*, void*, const char*, const char*, char*, size_t*)>("ReShadeGetConfigValue"))
            return fn(module, runtime, section, key, value, value_size);
        return false;
    }
    (void)module;
    (void)runtime;
    if (!ReadableString(key, 95)) return false;
    const char* sec = ReadableString(section, 63) ? section : "";

    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    Entry* e = FindEntry(sec, key);
    const bool firstSight = (e == nullptr) || !e->asked;
    if (!e) e = &TouchEntry(sec, key);
    e->asked = true;
    if (firstSight)
        Log("[addonhost] addon read config [%s] %s -> %s\n", sec, key,
            e->value.empty() ? "(unset: addon keeps its own default)" : e->value.c_str());

    // Unset means "we have nothing to say", and returning false is how the addon is told to keep
    // its built-in default. Returning an empty string instead would override it with nothing.
    if (e->value.empty()) return false;

    if (value == nullptr) {                       // size query
        if (value_size) *value_size = e->value.size() + 1;
        return true;
    }
    const size_t cap = value_size ? *value_size : 0;
    if (cap == 0) return false;
    const size_t n = (e->value.size() < cap - 1) ? e->value.size() : cap - 1;
    memcpy(value, e->value.c_str(), n);
    value[n] = '\0';
    if (value_size) *value_size = n;
    return true;
}

extern "C" __declspec(dllexport)
void ReShadeSetConfigValue(void* module, void* runtime, const char* section, const char* key,
                           const char* value) {
    if (!g_enabled) {
        if (auto fn = Forwarded<void(*)(void*, void*, const char*, const char*, const char*)>("ReShadeSetConfigValue"))
            fn(module, runtime, section, key, value);
        return;
    }
    (void)module;
    (void)runtime;
    if (!ReadableString(key, 95)) return;
    const char* sec = ReadableString(section, 63) ? section : "";
    const char* val = ReadableString(value, 191) ? value : "";

    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    Entry& e = TouchEntry(sec, key);
    const bool firstSight = !e.set;
    e.set = true;
    e.value = val;
    if (firstSight) Log("[addonhost] addon wrote config [%s] %s = %s\n", sec, key, val);
    // DEBOUNCED, and it has to be. This is called from the addon's settings page, which believes a
    // control changed whenever a widget says so -- during discovery that was every widget every
    // frame, and a full ini rewrite per frame is what brought the game to a crawl. The store is
    // authoritative in memory; the file catches up.
    g_iniDirty = true;
}

extern "C" __declspec(dllexport)
const void* ReShadeGetImGuiFunctionTable(uint32_t version) {
    if (!g_enabled) {
        if (auto fn = Forwarded<const void*(*)(uint32_t)>("ReShadeGetImGuiFunctionTable"))
            return fn(version);
        return nullptr;
    }
    if (g_imguiVersionAsked != version) {
        g_imguiVersionAsked = version;
        Log("[addonhost] addon asked for the ImGui function table for IMGUI_VERSION_NUM %u "
            "(%s).\n", version,
            g_overlayWidgets ? "exact live widget forwarding enabled" : "recording stubs only");
    }
    // Never null while hosting: reshade.hpp treats null as a hard failure and the addon would
    // abort before its NGX hooks were installed.
    return g_imguiTable;
}

// ================================================================================================
// Host lifecycle + the F10 ABI.
// ================================================================================================

extern "C" void CyberpunkVR_AddonHostInit() {
    {
        std::lock_guard<std::recursive_mutex> lock(g_mtx);
        // Load the persisted widget mode BEFORE constructing the table handed to the addon. The
        // previous order always built recording stubs from the compiled default, then read
        // overlay_widgets=1 too late. The F10 page looked enabled while the addon still held the
        // stub table, which effectively took all of its live controls away for that launch.
        LoadIni();
        BuildImGuiTable();
        ResolveScanDir();
        if (g_started) return;
        g_started = true;
    }
    if (!g_enabled) {
        Log("[addonhost] hosting disabled (set enabled=1 in %s, or use the F10 menu). Exported "
            "ReShade entry points will forward to a real ReShade if one is loaded.\n", g_iniPath);
        return;
    }
    if (FindOtherReShadeHost()) {
        g_enabled = false;
        CopyBounded(g_lastError, sizeof(g_lastError),
                    "ReShade is loaded in this process; hosting stood down to avoid loading every addon twice");
        Log("[addonhost] ReShade is already in this process -- standing down so addons are not "
            "loaded twice. Disable the ReShade OpenXR layer if you want the plugin to host them.\n");
        return;
    }
    { std::lock_guard<std::recursive_mutex> lock(g_mtx); SaveIniLocked(); }
    Log("[addonhost] hosting enabled; scanning %s for *.addon64 once NGX is up.\n", g_scanDir);
    std::thread(DeferredLoadThread).detach();
}

extern "C" void CyberpunkVR_AddonHostShutdown() {
    // ReShade event 1 is destroy_device. Newer DLSS5 addons subscribe to it so they can release
    // feature/resource registries and remove their NGX hooks before RED4ext unloads this host.
    // The old host recorded the callback but never delivered it, leaving addon code alive with
    // function pointers into a plugin that was about to unload.
    g_presentArmed.store(false, std::memory_order_release);
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (!g_initDeviceDelivered) return;

    for (const auto& e : g_events) {
        if (e.id != 1) continue; // destroy_device
        for (void* fn : e.fns) {
            Log("[addonhost] delivering destroy_device(device=%p) to %p ...\n", g_nativeDevice, fn);
            if (!InvokeDeviceEventGuarded(fn, g_fakeDevice))
                Log("[addonhost] destroy_device callback %p FAULTED; continuing process teardown.\n", fn);
        }
    }
    g_initDeviceDelivered = false;
    g_overlayCallback = nullptr;
    Log("[addonhost] destroy_device delivery complete.\n");
}

extern "C" int CyberpunkVR_AddonHostGetStatus(CyberpunkVRAddonHostStatus* out) {
    if (!out) return 0;
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    out->enabled          = g_enabled ? 1 : 0;
    out->forwarding       = (!g_enabled && g_forwardTo != nullptr) ? 1 : 0;
    out->addonsFound      = g_addonsFound;
    out->addonsLoaded     = g_addonsLoaded;
    out->addonsRegistered = g_addonsRegistered;
    out->apiVersion       = g_apiVersion;
    CopyBounded(out->addonName,        sizeof(out->addonName),        g_addonName);
    CopyBounded(out->addonDescription, sizeof(out->addonDescription), g_addonDesc);
    CopyBounded(out->scanDir,          sizeof(out->scanDir),          g_scanDir);
    CopyBounded(out->lastError,        sizeof(out->lastError),        g_lastError);
    CopyBounded(out->vtCalls,          sizeof(out->vtCalls),          g_vtCalls);
    out->imguiVersionAsked = g_imguiVersionAsked;
    out->initDeviceDelivered = g_initDeviceDelivered ? 1 : 0;
    out->wantsPresent = 0;
    for (const auto& e : g_events) if (e.id == 74) out->wantsPresent = 1;
    out->presentArmed   = g_presentArmed.load(std::memory_order_relaxed) ? 1 : 0;
    out->presentFaulted = g_presentFaulted.load(std::memory_order_relaxed) ? 1 : 0;
    out->presentCalls   = g_presentCalls.load(std::memory_order_relaxed);
    out->presentPerView = g_presentPerView;
    out->hasOverlay     = g_overlayCallback ? 1 : 0;
    out->drawOverlay    = g_drawOverlay ? 1 : 0;
    out->overlayFaulted = g_overlayFaulted ? 1 : 0;
    out->overlayStubReturn = g_overlayStubReturn;
    out->overlayWidgets    = g_overlayWidgets;
    CopyBounded(out->imguiCalls, sizeof(out->imguiCalls), g_imguiCalls);
    return 1;
}

extern "C" void CyberpunkVR_AddonHostSetEnabled(int enabled) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    g_enabled = (enabled != 0);
    SaveIniLocked();
    Log("[addonhost] hosting %s; takes effect on the next launch.\n", g_enabled ? "enabled" : "disabled");
}

extern "C" void CyberpunkVR_AddonHostOnPresent(void* swapChain) {
    if (!g_presentArmed.load(std::memory_order_acquire)) return;
    if (g_presentFaulted.load(std::memory_order_relaxed)) return;
    g_nativeSwap = reinterpret_cast<uint64_t>(swapChain);

    // No lock on this path: it is the present thread, once a frame, and the only things it reads
    // are set once before arming. Taking g_mtx here would put the overlay's status poll in front
    // of every frame the game draws.
    for (const auto& e : g_events) {
        if (e.id != 74) continue;
        for (void* fn : e.fns) {
            if (!InvokePresentGuarded(reinterpret_cast<PresentFn>(fn), g_fakeSwap)) {
                g_presentFaulted.store(true, std::memory_order_relaxed);
                Log("[addonhost] present callback %p FAULTED -- present dispatch disabled for this "
                    "session. Set dispatch_present=0 in reshade-addons.ini to stop arming it.\n", fn);
                return;
            }
            if (g_presentPerView) {
                // The VRCAM view, announced as its own swapchain. Its native handle is the VRCAM
                // context key rather than the real IDXGISwapChain, so a framework that keys off
                // get_native() rather than object identity still sees two distinct targets.
                g_nativeSwapVrcam = CyberpunkVR_VrcamCtxKey();
                if (!InvokePresentGuarded(reinterpret_cast<PresentFn>(fn), g_fakeSwapVrcam)) {
                    g_presentFaulted.store(true, std::memory_order_relaxed);
                    Log("[addonhost] per-view present FAULTED on the VRCAM swapchain -- dispatch "
                        "disabled. Set present_per_view=0 in reshade-addons.ini.\n");
                    return;
                }
            }
        }
    }
    if (g_iniDirty && GetTickCount() - g_lastIniWrite > 2000) {
        std::lock_guard<std::recursive_mutex> lock(g_mtx);
        g_iniDirty = false;
        g_lastIniWrite = GetTickCount();
        SaveIniLocked();
    }
    const uint64_t n = g_presentCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == 1 || n == 60 || n == 600) Log("[addonhost] present delivered %llu time(s).\n", n);
}

extern "C" int CyberpunkVR_AddonHostDrawOverlay() {
    if (!g_enabled || !g_drawOverlay || !g_overlayCallback || g_overlayFaulted) return 0;
    static bool s_vtFilled = false;
    if (!s_vtFilled) { FillRuntimeVt(std::make_integer_sequence<int, kVtSlots>{}); s_vtFilled = true; }
    if (!InvokeOverlayGuarded(reinterpret_cast<void(*)(void*)>(g_overlayCallback))) {
        g_overlayFaulted = true;
        Log("[addonhost] the addon's overlay FAULTED -- not called again this session. The last "
            "imgui_function_table slot logged above is the one that has to be real.\n");
        return 0;
    }
    return 1;
}

extern "C" void CyberpunkVR_AddonHostSetDrawOverlay(int on) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    g_drawOverlay = (on != 0);
    g_overlayFaulted = false;
    SaveIniLocked();
}

// Re-arms discovery: the label log is first-sight-only, so changing what the stubs return is
// pointless unless the page is allowed to announce itself again.
extern "C" void CyberpunkVR_AddonHostSetPresentPerView(int on) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    g_presentPerView = on ? 1 : 0;
    g_presentFaulted = false;
    SaveIniLocked();
    Log("[addonhost] per-view present %s. The addon builds per-swapchain state when it first sees "
        "a swapchain, so a codec it already created will not be rebuilt until the next launch.\n",
        g_presentPerView ? "ENABLED (VRCAM announced as a second swapchain)" : "disabled");
}

extern "C" void CyberpunkVR_AddonHostSetOverlayWidgets(int on) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    g_overlayWidgets = on ? 1 : 0;
    if (g_overlayWidgets) g_overlayStubReturn = 0;   // see the note on the flag
    BuildImGuiTable();
    g_overlayFaulted = false;
    SaveIniLocked();
    Log("[addonhost] real overlay widgets %s.\n", g_overlayWidgets ? "enabled" : "disabled");
}

extern "C" void CyberpunkVR_AddonHostSetStubReturn(int value) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (g_overlayWidgets && value) return;            // never both
    g_overlayStubReturn = value ? 1 : 0;
    memset(g_imguiSeen, 0, sizeof(g_imguiSeen));
    g_imguiCalls[0] = '\0';
    g_overlayFaulted = false;
    SaveIniLocked();
    Log("[addonhost] overlay stub return set to %d; slot discovery re-armed.\n", g_overlayStubReturn);
}

extern "C" int CyberpunkVR_AddonHostGetEntryCount() {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    return static_cast<int>(g_entries.size());
}

extern "C" int CyberpunkVR_AddonHostGetEntry(int index, CyberpunkVRAddonEntry* out) {
    if (!out) return 0;
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (index < 0 || index >= static_cast<int>(g_entries.size())) return 0;
    const Entry& e = g_entries[static_cast<size_t>(index)];
    CopyBounded(out->section, sizeof(out->section), e.section.c_str());
    CopyBounded(out->key,     sizeof(out->key),     e.key.c_str());
    CopyBounded(out->value,   sizeof(out->value),   e.value.c_str());
    out->askedByAddon = e.asked ? 1 : 0;
    out->setByAddon   = e.set   ? 1 : 0;
    return 1;
}

extern "C" void CyberpunkVR_AddonHostSetEntry(const char* section, const char* key, const char* value) {
    if (!key || !*key) return;
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    Entry& e = TouchEntry(section ? section : "", key);
    e.value = value ? value : "";
    SaveIniLocked();
}

extern "C" int CyberpunkVR_AddonHostGetEventCount() {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    return static_cast<int>(g_events.size());
}

extern "C" int CyberpunkVR_AddonHostGetEvent(int index, unsigned* outEventId, int* outCallbacks) {
    std::lock_guard<std::recursive_mutex> lock(g_mtx);
    if (index < 0 || index >= static_cast<int>(g_events.size())) return 0;
    if (outEventId)   *outEventId   = g_events[static_cast<size_t>(index)].id;
    if (outCallbacks) *outCallbacks = g_events[static_cast<size_t>(index)].callbacks;
    return 1;
}
