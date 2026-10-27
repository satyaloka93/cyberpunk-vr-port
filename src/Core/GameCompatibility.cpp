#include "Core/GameCompatibility.hpp"

#include <windows.h>
#include <vector>

extern void Log(const char* fmt, ...);

namespace cvr::compatibility {

bool CheckGameCompatibility() {
    wchar_t path[32768]{};
    GameBuild actual{};
    bool read = false;
    const DWORD length = GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
    if (length && length < std::size(path)) {
        const DWORD size = GetFileVersionInfoSizeW(path, nullptr);
        if (size) {
            std::vector<unsigned char> data(size);
            VS_FIXEDFILEINFO* version = nullptr;
            UINT versionSize = 0;
            if (GetFileVersionInfoW(path, 0, size, data.data()) &&
                VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&version), &versionSize) &&
                version && versionSize >= sizeof(*version) && version->dwSignature == 0xFEEF04BD) {
                actual = {HIWORD(version->dwFileVersionMS), LOWORD(version->dwFileVersionMS),
                          HIWORD(version->dwFileVersionLS), LOWORD(version->dwFileVersionLS)};
                read = true;
            }
        }
    }

    const auto status = Evaluate(read, actual);
    Log("[compat] executable=%u.%u.%u.%u SDK=%u.%u.%u.%u verified-engine-hooks=%u.%u.%u.%u\n",
        actual.major, actual.minor, actual.build, actual.revision,
        SdkBuild.major, SdkBuild.minor, SdkBuild.build, SdkBuild.revision,
        VerifiedEngineBuild.major, VerifiedEngineBuild.minor,
        VerifiedEngineBuild.build, VerifiedEngineBuild.revision);
    switch (status) {
    case Status::Ready:
        return true;
    case Status::UnknownExecutable:
        Log("[compat] REFUSING startup: cannot read the executable's numeric version.\n");
        break;
    case Status::SdkMismatch:
        Log("[compat] REFUSING startup: this binary's SDK layouts do not support the executable.\n");
        break;
    case Status::EngineHooksMismatch:
        Log("[compat] REFUSING startup: SDK migration is built, but private engine hooks/layouts "
            "are not validated for this game. No natives or engine hooks installed.\n");
        break;
    }
    return false;
}

} // namespace cvr::compatibility
