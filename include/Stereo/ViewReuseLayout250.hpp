#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cvr::stereo::reuse_layout {

// Exact 2.31/2.50 native evidence is in reference-231/view-reuse-migration.
// The preceding atmosphere block gains a DWORD at40C; its byte moves410,
// then alignment moves the following environment block410->420. This adapter
// is only for the legacy view-data ranges retained in ViewReuse.cpp.
constexpr uint16_t ViewDataOffset(uint16_t old) {
    return old >= 0x410 ? old + 0x10 : old >= 0x40C ? old + 4 : old;
}
constexpr bool RangeIsContiguous(uint16_t old, uint16_t length) {
    return length && ViewDataOffset(old + length - 1) == ViewDataOffset(old) + length - 1;
}
inline constexpr uintptr_t ViewDataGetterRva = 0x33C248;
inline constexpr size_t ViewDataSnapshotBytes = 0x1000; // capture bound, not an allocation assertion
inline constexpr uintptr_t RendererFrame = 0x4CC8;
inline constexpr uintptr_t SceneManager = 0x2C40;
inline constexpr uintptr_t CascadeRecordBase = 0xDDF50;
inline constexpr uint32_t CascadeRecordStride = 0x490;
inline constexpr uintptr_t CascadeBias = 0xA78;
inline constexpr uintptr_t CloudInput = 0x440;

// The first3E0 bytes of a cascade record are the native camera descriptor.
// The new tail can own children. Lend only plain source/destination records,
// preserving their first16 bytes as the existing same-frame fit policy does.
inline bool PlainCascadeRecord(const uint8_t* record) {
    if (!record || record[0x399] != 0) return false;
    uint64_t child0{}, child1{};
    std::memcpy(&child0, record + 0x3D0, 8);
    std::memcpy(&child1, record + 0x3D8, 8);
    return child0 == 0 && child1 == 0;
}
inline bool LendPlainCascadeFit(uint8_t* destination, const uint8_t* source) {
    if (!PlainCascadeRecord(source) || !PlainCascadeRecord(destination)) return false;
    std::memcpy(destination + 16, source + 16, CascadeRecordStride - 16);
    return true;
}

static_assert(!RangeIsContiguous(0x400, 0x60));
static_assert(RangeIsContiguous(0x400, 0xC));
static_assert(RangeIsContiguous(0x40C, 4));
static_assert(RangeIsContiguous(0x410, 0x50));

} // namespace cvr::stereo::reuse_layout
