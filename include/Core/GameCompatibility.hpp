#pragma once

#include <cstdint>

namespace cvr::compatibility {

struct GameBuild {
    uint16_t major, minor, build, revision;
    constexpr bool operator==(const GameBuild&) const = default;
};

inline constexpr GameBuild Game231{3, 0, 80, 51928};
inline constexpr GameBuild Game250{3, 0, 88, 46235};

// SDK object layouts and private renderer hooks are independent compatibility
// requirements. An SDK rebuild does not move or validate the engine detours.
inline constexpr GameBuild SdkBuild = Game250;
// Paired 2.31/2.50 native migration reviewed for the normal VR path. This
// selects the exact local test profile; it does not imply runtime acceptance.
inline constexpr GameBuild VerifiedEngineBuild = Game250;

enum class Status { Ready, UnknownExecutable, SdkMismatch, EngineHooksMismatch };

constexpr Status Evaluate(bool versionRead, GameBuild actual,
                          GameBuild sdk = SdkBuild,
                          GameBuild engine = VerifiedEngineBuild) {
    if (!versionRead) return Status::UnknownExecutable;
    if (!(actual == sdk)) return Status::SdkMismatch;
    if (!(actual == engine)) return Status::EngineHooksMismatch;
    return Status::Ready;
}

// Log the measured executable and reject before natives, engine hooks, worker
// threads or first-launch settings are touched. There is no force-load option.
bool CheckGameCompatibility();

} // namespace cvr::compatibility
