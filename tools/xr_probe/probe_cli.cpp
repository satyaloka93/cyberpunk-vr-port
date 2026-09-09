// xrprobe.exe - a minimal OpenXR application whose only purpose is to make the runtime say
// everything it knows, then leave.
//
// Two jobs. First, it is the smoke test for the layer: it walks the entire call sequence the layer
// intercepts (instance, system, view configuration, swapchain, session, frame loop, submit), so a
// broken hook shows up here in two seconds instead of inside a game.
//
// Second, and more useful day to day: it answers "what does this headset actually report" without
// launching anything. Point the OpenXR runtime at a real headset or at the Meta XR Simulator with
// a Quest 3 profile, run this, and the per-eye resolution, FOV, eye poses and IPD are on stdout.
// That is the reference the mod's own numbers get compared against.
//
// D3D11 is used for the session because it is the cheapest binding that every runtime supports;
// nothing is rendered. If session creation fails the tool still prints everything reachable
// without one -- system properties and the view configuration, which is most of it.

#define XR_USE_GRAPHICS_API_D3D11

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {

constexpr float kRad2Deg = 57.2957795131f;

XrInstance g_instance = XR_NULL_HANDLE;

const char* ResultName(XrResult r) {
    static char buf[XR_MAX_RESULT_STRING_SIZE];
    if (g_instance != XR_NULL_HANDLE && XR_SUCCEEDED(xrResultToString(g_instance, r, buf))) {
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "XrResult(%d)", r);
    return buf;
}

bool Check(XrResult r, const char* what) {
    if (XR_SUCCEEDED(r)) return true;
    std::printf("  !! %s failed: %s\n", what, ResultName(r));
    return false;
}

struct Euler { float yaw, pitch, roll; };

Euler QuatToEuler(const XrQuaternionf& q) {
    const float sinp = 2.0f * (q.w * q.x - q.y * q.z);
    const float pitch = (sinp >= 1.0f) ? 1.57079633f
                      : (sinp <= -1.0f) ? -1.57079633f : std::asin(sinp);
    return { std::atan2(2.0f * (q.w * q.y + q.z * q.x),
                        1.0f - 2.0f * (q.x * q.x + q.y * q.y)) * kRad2Deg,
             pitch * kRad2Deg,
             std::atan2(2.0f * (q.w * q.z + q.x * q.y),
                        1.0f - 2.0f * (q.z * q.z + q.x * q.x)) * kRad2Deg };
}

void PrintFov(const char* label, const XrFovf& f) {
    std::printf("  %s\n", label);
    std::printf("    rad   L %+.6f  R %+.6f  U %+.6f  D %+.6f\n",
                f.angleLeft, f.angleRight, f.angleUp, f.angleDown);
    std::printf("    deg   L %+.3f  R %+.3f  U %+.3f  D %+.3f     H %.3f  V %.3f\n",
                f.angleLeft * kRad2Deg, f.angleRight * kRad2Deg,
                f.angleUp * kRad2Deg, f.angleDown * kRad2Deg,
                (f.angleRight - f.angleLeft) * kRad2Deg,
                (f.angleUp - f.angleDown) * kRad2Deg);
    std::printf("    tan   L %.6f  R %.6f  U %.6f  D %.6f\n",
                std::tan(f.angleLeft), std::tan(f.angleRight),
                std::tan(f.angleUp), std::tan(f.angleDown));
    // Zero means an on-axis frustum. A canted panel reports a non-zero value here, and that is
    // precisely what "parallel projections" removes by folding it into the eye rotation.
    std::printf("    asym  H %+.4f deg   V %+.4f deg\n",
                (f.angleRight + f.angleLeft) * kRad2Deg,
                (f.angleUp + f.angleDown) * kRad2Deg);
}

}  // namespace

int main(int argc, char** argv) {
    int wantFrames = 60;
    for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--frames=", 9) == 0) wantFrames = std::atoi(argv[i] + 9);
    }
    int submitted = 0;
    bool dumped = false;

    std::printf("xrprobe -- reading the active OpenXR runtime\n");
    std::printf("===========================================\n\n");

    // ── layers and extensions on offer ───────────────────────────────────────────────────────
    uint32_t layerCount = 0;
    xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
    std::vector<XrApiLayerProperties> layers(layerCount, { XR_TYPE_API_LAYER_PROPERTIES });
    if (layerCount) xrEnumerateApiLayerProperties(layerCount, &layerCount, layers.data());
    std::printf("API layers visible to the loader (%u)\n", layerCount);
    for (const auto& l : layers) std::printf("  %s  -- %s\n", l.layerName, l.description);
    if (!layerCount) std::printf("  (none)\n");
    std::printf("\n");

    uint32_t extCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
    std::vector<XrExtensionProperties> exts(extCount, { XR_TYPE_EXTENSION_PROPERTIES });
    if (extCount) xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, exts.data());
    bool haveD3D11 = false;
    bool haveEyeGaze = false;   // XR_EXT_eye_gaze_interaction -- the cross-vendor one
    for (const auto& e : exts) {
        if (std::strcmp(e.extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0) haveD3D11 = true;
        if (std::strcmp(e.extensionName, XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME) == 0)
            haveEyeGaze = true;
    }
    std::printf("Runtime extensions: %u (D3D11 binding %s)\n", extCount,
                haveD3D11 ? "available" : "MISSING");

    // EVERY extension, printed. The eye-tracking question is asked of this list first, and a
    // runtime that does not advertise the extension cannot be made to produce gaze by any amount
    // of application-side work -- so seeing the whole list is what separates "the driver is not
    // exposing it" from "something above the runtime is dropping it".
    std::printf("\nExtensions the runtime advertises:\n");
    for (const auto& e : exts) {
        const bool eyeish = std::strstr(e.extensionName, "eye") != nullptr ||
                            std::strstr(e.extensionName, "gaze") != nullptr ||
                            std::strstr(e.extensionName, "EYE") != nullptr;
        std::printf("  %s%s (v%u)\n", eyeish ? "* " : "  ", e.extensionName,
                    e.extensionVersion);
    }
    std::printf("\nEye tracking: %s%s\n\n",
                haveEyeGaze ? "XR_EXT_eye_gaze_interaction ADVERTISED"
                            : "XR_EXT_eye_gaze_interaction NOT ADVERTISED",
                haveEyeGaze ? "" : "  <- the runtime is not offering eye gaze at all");

    // ── instance ─────────────────────────────────────────────────────────────────────────────
    // Eye gaze is requested only when advertised: naming an unsupported extension makes
    // xrCreateInstance fail outright, which would lose every other answer this tool gives.
    const char* wanted[3] = {};
    uint32_t wantedCount = 0;
    if (haveD3D11)   wanted[wantedCount++] = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
    if (haveEyeGaze) wanted[wantedCount++] = XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME;

    XrInstanceCreateInfo ici{ XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy_s(ici.applicationInfo.applicationName, "xrprobe");
    ici.applicationInfo.applicationVersion = 1;
    strcpy_s(ici.applicationInfo.engineName, "CyberpunkVRPort");
    ici.applicationInfo.engineVersion = 1;
    ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    ici.enabledExtensionCount = wantedCount;
    ici.enabledExtensionNames = wantedCount ? wanted : nullptr;

    if (!Check(xrCreateInstance(&ici, &g_instance), "xrCreateInstance")) return 1;

    XrInstanceProperties ip{ XR_TYPE_INSTANCE_PROPERTIES };
    if (Check(xrGetInstanceProperties(g_instance, &ip), "xrGetInstanceProperties")) {
        std::printf("Runtime: %s  v%llu.%llu.%llu\n\n", ip.runtimeName,
                    static_cast<unsigned long long>(XR_VERSION_MAJOR(ip.runtimeVersion)),
                    static_cast<unsigned long long>(XR_VERSION_MINOR(ip.runtimeVersion)),
                    static_cast<unsigned long long>(XR_VERSION_PATCH(ip.runtimeVersion)));
    }

    XrSystemGetInfo sgi{ XR_TYPE_SYSTEM_GET_INFO };
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    if (!Check(xrGetSystem(g_instance, &sgi, &systemId), "xrGetSystem")) {
        xrDestroyInstance(g_instance);
        return 1;
    }

    // THE RUNTIME'S OWN ANSWER, chained onto the system query. Advertising the extension only says
    // the runtime implements the API; supportsEyeGazeInteraction says THIS headset, with the driver
    // currently loaded, actually has an eye tracker behind it. The two differ exactly when a driver
    // is the thing at fault, which is the case being tested here.
    XrSystemEyeGazeInteractionPropertiesEXT eyeProps{
        XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT };
    XrSystemProperties sp{ XR_TYPE_SYSTEM_PROPERTIES };
    if (haveEyeGaze) sp.next = &eyeProps;
    if (Check(xrGetSystemProperties(g_instance, systemId, &sp), "xrGetSystemProperties")) {
        if (haveEyeGaze) {
            std::printf("  eye gaze : supportsEyeGazeInteraction = %s\n",
                        eyeProps.supportsEyeGazeInteraction ? "TRUE" : "FALSE");
        }
        std::printf("System : %s\n", sp.systemName);
        std::printf("  vendorId 0x%08x   maxSwapchain %ux%u   maxLayers %u\n",
                    sp.vendorId, sp.graphicsProperties.maxSwapchainImageWidth,
                    sp.graphicsProperties.maxSwapchainImageHeight,
                    sp.graphicsProperties.maxLayerCount);
        std::printf("  tracking: orientation %d  position %d\n\n",
                    sp.trackingProperties.orientationTracking ? 1 : 0,
                    sp.trackingProperties.positionTracking ? 1 : 0);
    }

    // ── view configuration: this is where the per-eye resolution lives ───────────────────────
    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(g_instance, systemId,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                      0, &viewCount, nullptr);
    std::vector<XrViewConfigurationView> vcv(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    if (viewCount) {
        xrEnumerateViewConfigurationViews(g_instance, systemId,
                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                          viewCount, &viewCount, vcv.data());
    }
    std::printf("View configuration PRIMARY_STEREO (%u views)\n", viewCount);
    for (uint32_t i = 0; i < viewCount; ++i) {
        std::printf("  eye %u  recommended %ux%u (x%u)   max %ux%u (x%u)\n", i,
                    vcv[i].recommendedImageRectWidth, vcv[i].recommendedImageRectHeight,
                    vcv[i].recommendedSwapchainSampleCount,
                    vcv[i].maxImageRectWidth, vcv[i].maxImageRectHeight,
                    vcv[i].maxSwapchainSampleCount);
    }
    std::printf("\n");

    uint32_t blendCount = 0;
    xrEnumerateEnvironmentBlendModes(g_instance, systemId,
                                     XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                     0, &blendCount, nullptr);
    std::vector<XrEnvironmentBlendMode> blends(blendCount);
    if (blendCount) {
        xrEnumerateEnvironmentBlendModes(g_instance, systemId,
                                         XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                         blendCount, &blendCount, blends.data());
    }

    if (!haveD3D11 || viewCount < 1) {
        std::printf("No D3D11 binding -- stopping before the session. Everything above is still valid.\n");
        xrDestroyInstance(g_instance);
        return 0;
    }

    // ── D3D11 device on the adapter the runtime demands ──────────────────────────────────────
    auto xrGetD3D11GraphicsRequirementsKHR_ = PFN_xrGetD3D11GraphicsRequirementsKHR(nullptr);
    xrGetInstanceProcAddr(g_instance, "xrGetD3D11GraphicsRequirementsKHR",
                          reinterpret_cast<PFN_xrVoidFunction*>(&xrGetD3D11GraphicsRequirementsKHR_));
    XrGraphicsRequirementsD3D11KHR req{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    if (!xrGetD3D11GraphicsRequirementsKHR_ ||
        !Check(xrGetD3D11GraphicsRequirementsKHR_(g_instance, systemId, &req),
               "xrGetD3D11GraphicsRequirementsKHR")) {
        xrDestroyInstance(g_instance);
        return 1;
    }

    IDXGIFactory1* factory = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        IDXGIAdapter1* cur = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &cur) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            cur->GetDesc1(&desc);
            if (std::memcmp(&desc.AdapterLuid, &req.adapterLuid, sizeof(LUID)) == 0) {
                adapter = cur;
                break;
            }
            cur->Release();
        }
    }

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL levels[] = { req.minFeatureLevel };
    if (FAILED(D3D11CreateDevice(adapter,
                                 adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                 nullptr, 0, levels, 1, D3D11_SDK_VERSION,
                                 &device, nullptr, &context))) {
        std::printf("  !! D3D11CreateDevice failed\n");
        if (adapter) adapter->Release();
        if (factory) factory->Release();
        xrDestroyInstance(g_instance);
        return 1;
    }

    XrGraphicsBindingD3D11KHR binding{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    binding.device = device;

    XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO };
    sci.next = &binding;
    sci.systemId = systemId;
    XrSession session = XR_NULL_HANDLE;
    if (!Check(xrCreateSession(g_instance, &sci, &session), "xrCreateSession")) {
        xrDestroyInstance(g_instance);
        return 1;
    }

    XrReferenceSpaceCreateInfo rsci{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace.orientation.w = 1.0f;
    XrSpace localSpace = XR_NULL_HANDLE;
    Check(xrCreateReferenceSpace(session, &rsci, &localSpace), "xrCreateReferenceSpace(LOCAL)");

    // A swapchain sized exactly as recommended, so the layer records a realistic create.
    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    if (formatCount) xrEnumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());

    XrSwapchainCreateInfo scci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    scci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    scci.format = formatCount ? formats[0] : 29 /* R8G8B8A8_UNORM_SRGB */;
    scci.width = vcv[0].recommendedImageRectWidth;
    scci.height = vcv[0].recommendedImageRectHeight;
    scci.sampleCount = 1;
    scci.faceCount = 1;
    scci.arraySize = 2;
    scci.mipCount = 1;
    XrSwapchain swapchain = XR_NULL_HANDLE;
    Check(xrCreateSwapchain(session, &scci, &swapchain), "xrCreateSwapchain");

    // ── eye gaze action, bound the way the spec requires ─────────────────────────────────────
    //
    // supportsEyeGazeInteraction is a claim; this is the proof. Gaze arrives as a POSE ACTION on
    // the /interaction_profiles/ext/eye_gaze_interaction profile, so it needs an action set, a
    // suggested binding, an attached set, xrSyncActions every frame, and an action space to
    // locate. A runtime can advertise the extension, answer TRUE, and still never mark the pose
    // TRACKED -- which is exactly what a driver that is not delivering samples looks like, and it
    // cannot be distinguished any other way.
    XrActionSet gazeSet = XR_NULL_HANDLE;
    XrAction gazeAction = XR_NULL_HANDLE;
    XrSpace gazeSpace = XR_NULL_HANDLE;
    if (haveEyeGaze) {
        XrActionSetCreateInfo asci{ XR_TYPE_ACTION_SET_CREATE_INFO };
        strcpy_s(asci.actionSetName, "gaze_probe");
        strcpy_s(asci.localizedActionSetName, "Gaze Probe");
        asci.priority = 0;
        if (Check(xrCreateActionSet(g_instance, &asci, &gazeSet), "xrCreateActionSet(gaze)")) {
            XrActionCreateInfo aci{ XR_TYPE_ACTION_CREATE_INFO };
            strcpy_s(aci.actionName, "gaze_pose");
            strcpy_s(aci.localizedActionName, "Gaze Pose");
            aci.actionType = XR_ACTION_TYPE_POSE_INPUT;
            Check(xrCreateAction(gazeSet, &aci, &gazeAction), "xrCreateAction(gaze_pose)");
        }
        if (gazeAction) {
            XrPath profile = XR_NULL_PATH, gazePath = XR_NULL_PATH;
            xrStringToPath(g_instance, "/interaction_profiles/ext/eye_gaze_interaction", &profile);
            xrStringToPath(g_instance, "/user/eyes_ext/input/gaze_ext/pose", &gazePath);
            XrActionSuggestedBinding bind{ gazeAction, gazePath };
            XrInteractionProfileSuggestedBinding sb{
                XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
            sb.interactionProfile = profile;
            sb.countSuggestedBindings = 1;
            sb.suggestedBindings = &bind;
            Check(xrSuggestInteractionProfileBindings(g_instance, &sb),
                  "xrSuggestInteractionProfileBindings(eye_gaze)");

            XrSessionActionSetsAttachInfo attach{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
            attach.countActionSets = 1;
            attach.actionSets = &gazeSet;
            Check(xrAttachSessionActionSets(session, &attach), "xrAttachSessionActionSets");

            XrActionSpaceCreateInfo asp{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
            asp.action = gazeAction;
            asp.poseInActionSpace.orientation.w = 1.0f;
            Check(xrCreateActionSpace(session, &asp, &gazeSpace), "xrCreateActionSpace(gaze)");
        }
    }

    // ── wait for READY, then run the frame loop ──────────────────────────────────────────────
    // FOCUS IS A PRECONDITION FOR ACTIONS, not a detail. xrSyncActions returns
    // XR_SESSION_NOT_FOCUSED and every action stays inactive unless the session is FOCUSED, so a
    // probe that tops out at VISIBLE reads a perfectly healthy eye tracker as flags=0x0. The first
    // run of this tool did exactly that and the verdict blamed the driver. Track the state, count
    // only focused frames, and say so when focus never arrives.
    unsigned gazeValid = 0, gazeTracked = 0, gazeFrames = 0, focusedFrames = 0;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool reachedFocus = false;
    XrResult lastSyncResult = XR_SUCCESS;
    bool running = false, done = false;
    int guard = 0;
    while (!done && guard++ < 2000) {
        XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
        while (xrPollEvent(g_instance, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* s = reinterpret_cast<const XrEventDataSessionStateChanged*>(&ev);
                sessionState = s->state;
                if (s->state == XR_SESSION_STATE_FOCUSED) reachedFocus = true;
                std::printf("  session state -> %d%s\n", static_cast<int>(s->state),
                            s->state == XR_SESSION_STATE_FOCUSED ? "  (FOCUSED -- actions live)"
                                                                 : "");
                if (s->state == XR_SESSION_STATE_READY && !running) {
                    XrSessionBeginInfo sbi{ XR_TYPE_SESSION_BEGIN_INFO };
                    sbi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (Check(xrBeginSession(session, &sbi), "xrBeginSession")) running = true;
                } else if (s->state == XR_SESSION_STATE_STOPPING ||
                           s->state == XR_SESSION_STATE_EXITING ||
                           s->state == XR_SESSION_STATE_LOSS_PENDING) {
                    done = true;
                }
            }
            ev = { XR_TYPE_EVENT_DATA_BUFFER };
        }
        if (!running) { Sleep(5); continue; }

        XrFrameState fs{ XR_TYPE_FRAME_STATE };
        if (!Check(xrWaitFrame(session, nullptr, &fs), "xrWaitFrame")) break;
        Check(xrBeginFrame(session, nullptr), "xrBeginFrame");

        // Gaze, sampled every frame. xrSyncActions first -- an action space locates to nothing
        // without it, and that omission reads identically to a headset with no eye tracker.
        if (gazeSpace && gazeSet) {
            XrActiveActionSet active{ gazeSet, XR_NULL_PATH };
            XrActionsSyncInfo sync{ XR_TYPE_ACTIONS_SYNC_INFO };
            sync.countActiveActionSets = 1;
            sync.activeActionSets = &active;
            lastSyncResult = xrSyncActions(session, &sync);
            if (sessionState == XR_SESSION_STATE_FOCUSED) ++focusedFrames;

            XrSpaceLocation loc{ XR_TYPE_SPACE_LOCATION };
            if (xrLocateSpace(gazeSpace, localSpace, fs.predictedDisplayTime, &loc) == XR_SUCCESS) {
                ++gazeFrames;
                const bool valid = (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
                const bool tracked =
                    (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0;
                if (valid)   ++gazeValid;
                if (tracked) ++gazeTracked;
                if (gazeFrames <= 3 || (tracked && gazeTracked <= 3)) {
                    std::printf("  gaze frame %u: flags=0x%llX valid=%d tracked=%d "
                                "quat=(%.3f %.3f %.3f %.3f)\n",
                                gazeFrames,
                                static_cast<unsigned long long>(loc.locationFlags),
                                valid ? 1 : 0, tracked ? 1 : 0,
                                loc.pose.orientation.x, loc.pose.orientation.y,
                                loc.pose.orientation.z, loc.pose.orientation.w);
                }
            }
        }

        XrViewLocateInfo vli{ XR_TYPE_VIEW_LOCATE_INFO };
        vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        vli.displayTime = fs.predictedDisplayTime;
        vli.space = localSpace;
        XrViewState vs{ XR_TYPE_VIEW_STATE };
        uint32_t got = 0;
        std::vector<XrView> views(viewCount, { XR_TYPE_VIEW });
        const XrResult lv = xrLocateViews(session, &vli, &vs, viewCount, &got, views.data());

        if (!dumped && XR_SUCCEEDED(lv) && got >= 2) {
            std::printf("\nEye geometry (frame %d, displayTime %lld, period %.3f ms / %.1f Hz)\n",
                        submitted, static_cast<long long>(fs.predictedDisplayTime),
                        fs.predictedDisplayPeriod / 1.0e6,
                        fs.predictedDisplayPeriod ? 1.0e9 / fs.predictedDisplayPeriod : 0.0);
            for (uint32_t i = 0; i < got && i < 2; ++i) {
                const Euler e = QuatToEuler(views[i].pose.orientation);
                std::printf("  %s eye  pos (%+.4f, %+.4f, %+.4f)  yaw %+.3f  pitch %+.3f  roll %+.3f\n",
                            i == 0 ? "left " : "right",
                            views[i].pose.position.x, views[i].pose.position.y,
                            views[i].pose.position.z, e.yaw, e.pitch, e.roll);
                PrintFov(i == 0 ? "  left  fov" : "  right fov", views[i].fov);
            }
            const float dx = views[0].pose.position.x - views[1].pose.position.x;
            const float dy = views[0].pose.position.y - views[1].pose.position.y;
            const float dz = views[0].pose.position.z - views[1].pose.position.z;
            const float ipd = std::sqrt(dx * dx + dy * dy + dz * dz);
            // Both cant conventions, because runtimes disagree about which one to use and reading
            // only the pose misses a frustum-encoded cant entirely. See the long note in layer.cpp.
            const Euler l = QuatToEuler(views[0].pose.orientation);
            const Euler r = QuatToEuler(views[1].pose.orientation);
            const float poseCant = 0.5f * (std::fabs(l.yaw) + std::fabs(r.yaw));
            const float asymL = (views[0].fov.angleRight + views[0].fov.angleLeft) * kRad2Deg;
            const float asymR = (views[1].fov.angleRight + views[1].fov.angleLeft) * kRad2Deg;
            const float frustumCant = 0.25f * (std::fabs(asymL) + std::fabs(asymR));
            std::printf("  IPD %.4f m (%.1f mm)\n", ipd, ipd * 1000.0f);
            std::printf("  cant: %.3f deg as eye yaw, %.3f deg as FOV asymmetry -> %s\n",
                        poseCant, frustumCant,
                        (poseCant + frustumCant > 0.2f) ? "CANTED panels" : "parallel/symmetric panels");
            if (frustumCant > 0.2f && poseCant <= 0.2f) {
                std::printf("  NOTE: cant lives entirely in the frustum, not the eye pose.\n");
            }
            dumped = true;   // one full dump is enough; keep looping quietly to exercise submit
        }

        std::vector<XrCompositionLayerProjectionView> pv(got, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });
        for (uint32_t i = 0; i < got; ++i) {
            pv[i].pose = views[i].pose;
            pv[i].fov = views[i].fov;
            pv[i].subImage.swapchain = swapchain;
            pv[i].subImage.imageArrayIndex = i;
            pv[i].subImage.imageRect.offset = { 0, 0 };
            pv[i].subImage.imageRect.extent = { static_cast<int32_t>(scci.width),
                                                static_cast<int32_t>(scci.height) };
        }
        XrCompositionLayerProjection proj{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        proj.space = localSpace;
        proj.viewCount = got;
        proj.views = pv.data();
        const XrCompositionLayerBaseHeader* layerList[] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj)
        };

        // The swapchain image must be acquired and released even though nothing is drawn, or the
        // runtime rejects the layer.
        uint32_t imageIndex = 0;
        XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        wi.timeout = XR_INFINITE_DURATION;
        const bool acquired = XR_SUCCEEDED(xrAcquireSwapchainImage(swapchain, &ai, &imageIndex)) &&
                              XR_SUCCEEDED(xrWaitSwapchainImage(swapchain, &wi));
        if (acquired) {
            XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
            xrReleaseSwapchainImage(swapchain, &ri);
        }

        XrFrameEndInfo fei{ XR_TYPE_FRAME_END_INFO };
        fei.displayTime = fs.predictedDisplayTime;
        fei.environmentBlendMode = blendCount ? blends[0] : XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fei.layerCount = (fs.shouldRender && acquired && got >= 2) ? 1u : 0u;
        fei.layers = fei.layerCount ? layerList : nullptr;
        xrEndFrame(session, &fei);

        // Don't stop the moment the frame budget is met: 60 frames is two thirds of a second, and
        // focus routinely arrives later than that. Once focused, take 60 FOCUSED frames -- those
        // are the only ones that can carry gaze. If focus never comes, give up at ~10 s rather
        // than reporting a verdict from frames that were never entitled to action data.
        ++submitted;
        if (submitted >= wantFrames &&
            (reachedFocus ? focusedFrames >= 60u : submitted >= 900)) {
            done = true;
        }
    }

    // ── the eye-tracking verdict, in one place ───────────────────────────────────────────────
    // Four distinguishable outcomes, and each points somewhere different. Printed last so it is
    // the thing still on screen when the tool exits.
    std::printf("\n=== EYE TRACKING ===\n");
    if (!haveEyeGaze) {
        std::printf("  XR_EXT_eye_gaze_interaction: NOT ADVERTISED by the runtime.\n"
                    "  Nothing above the runtime can supply gaze. Check that the driver providing\n"
                    "  eye tracking is loaded and that this runtime is the one it plugs into.\n");
    } else if (gazeFrames == 0) {
        std::printf("  Extension advertised, but the gaze space never located.\n"
                    "  The action never bound -- look at the xrSuggestInteractionProfileBindings\n"
                    "  and xrAttachSessionActionSets results above.\n");
    } else if (!reachedFocus) {
        std::printf("  INCONCLUSIVE -- the session never reached FOCUSED (stopped at state %d).\n"
                    "  OpenXR delivers no action data outside FOCUSED: xrSyncActions returns\n"
                    "  XR_SESSION_NOT_FOCUSED and every action stays inactive, so gaze reads\n"
                    "  flags=0x0 even from a perfectly working tracker. Last xrSyncActions = %d.\n"
                    "  Close the SteamVR dashboard, put the headset ON so user presence is\n"
                    "  detected, and run again -- focus is usually withheld for one of those.\n",
                    static_cast<int>(sessionState), static_cast<int>(lastSyncResult));
    } else if (gazeTracked == 0) {
        std::printf("  Extension advertised, bound, and the session WAS focused (%u focused\n"
                    "  frame(s) of %u located), but TRACKED on 0.\n"
                    "  The runtime accepts the API and returns no gaze samples -- the signature of\n"
                    "  a driver that is not delivering eye data, or eyes not detected in-headset.\n",
                    focusedFrames, gazeFrames);
    } else {
        std::printf("  WORKING: %u/%u frames tracked (%u valid).\n"
                    "  Eye tracking reaches OpenXR on this setup. Anything that cannot see gaze is\n"
                    "  failing above the runtime, not below it.\n",
                    gazeTracked, gazeFrames, gazeValid);
    }

    if (running) xrEndSession(session);
    if (gazeSpace) xrDestroySpace(gazeSpace);
    if (gazeAction) xrDestroyAction(gazeAction);
    if (gazeSet) xrDestroyActionSet(gazeSet);
    if (swapchain) xrDestroySwapchain(swapchain);
    if (localSpace) xrDestroySpace(localSpace);
    if (session) xrDestroySession(session);
    if (context) context->Release();
    if (device) device->Release();
    if (adapter) adapter->Release();
    if (factory) factory->Release();
    xrDestroyInstance(g_instance);

    std::printf("\ndone. If the probe layer was active, its .jsonl and .txt are in %%LOCALAPPDATA%%\\xrprobe\n");
    return 0;
}
