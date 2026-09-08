#include "Core/XrLayerOverrides.hpp"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <share.h>

extern void Log(const char* fmt, ...);

namespace {

// The layers whose disable_environment names we set when vrport.ini says nothing. Only ReShade's
// is on by default, and only because its layer is PROVEN to end this process on the first
// xrEndFrame -- see the header. The other two are listed so that turning them off is a matter of
// copying a name rather than finding one:
//
//   ReShade   XR_APILAYER_reshade                  DISABLE_XR_APILAYER_reshade_1
//   Cheeky    XR_APILAYER_CHEEKY_foveated_dlss     CHEEKY_OPENXR_LAYER_DISABLE
//   OFXR      XR_APILAYER_XRFrameBridge_diagnostic XRFG_DISABLE_OFXR_BRIDGE
constexpr const char* kDefaultDisables = "DISABLE_XR_APILAYER_reshade_1";

// vrport.ini, flat key=value like everything else in it:
//
//   xr_disable_api_layers=DISABLE_XR_APILAYER_reshade_1;CHEEKY_OPENXR_LAYER_DISABLE
//
// An ABSENT key takes the default above. A key present but EMPTY disables nothing, which is how
// to get the stock layer chain back without deleting the line.
bool ReadDisableList(char* out, size_t size) {
    out[0] = '\0';

    char iniPath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, iniPath, MAX_PATH) == 0) return false;
    char* lastSlash = strrchr(iniPath, '\\');
    if (!lastSlash) return false;
    *(lastSlash + 1) = '\0';
    strncat_s(iniPath, MAX_PATH, "vrport.ini", _TRUNCATE);

    FILE* file = _fsopen(iniPath, "r", _SH_DENYNO);
    if (!file) return false;

    bool found = false;
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char* equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';
        if (_stricmp(line, "xr_disable_api_layers") != 0) continue;

        char* value = equals + 1;
        size_t len = strlen(value);
        while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r' ||
                           value[len - 1] == ' '  || value[len - 1] == '\t')) {
            value[--len] = '\0';
        }
        while (*value == ' ' || *value == '\t') ++value;
        strncpy_s(out, size, value, _TRUNCATE);
        found = true;
        break;
    }
    fclose(file);
    return found;
}

} // namespace

void ApplyOpenXrLayerOverrides() {
    char configured[1024];
    const bool explicitKey = ReadDisableList(configured, sizeof(configured));

    char list[1024];
    strncpy_s(list, sizeof(list), explicitKey ? configured : kDefaultDisables, _TRUNCATE);

    if (list[0] == '\0') {
        Log("[XR] api-layer overrides: none (xr_disable_api_layers is empty)\n");
        return;
    }

    int applied = 0;
    char* context = nullptr;
    for (char* name = strtok_s(list, ";,", &context); name != nullptr;
         name = strtok_s(nullptr, ";,", &context)) {
        while (*name == ' ' || *name == '\t') ++name;
        size_t len = strlen(name);
        while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t')) name[--len] = '\0';
        if (*name == '\0') continue;

        // The loader disables a layer when its disable_environment name is DEFINED; the value is
        // not read. "1" is set for the benefit of anyone reading the environment by hand.
        if (SetEnvironmentVariableA(name, "1")) {
            Log("[XR] api-layer override: %s set for this process only\n", name);
            ++applied;
        } else {
            Log("[XR] api-layer override: %s FAILED (err=%lu)\n", name, GetLastError());
        }
    }

    Log("[XR] api-layer overrides applied: %d (%s)\n", applied,
        explicitKey ? "from vrport.ini" : "default");
}
