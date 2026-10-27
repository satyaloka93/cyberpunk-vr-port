#pragma once
#include <cstdint>

namespace cvr::stereo::resource_layout {
// CP2077 2.50: resolver334810, declaration334A34 and compiler3E0330.
// Paired instruction evidence: reference-231/resource-migration/. The slot
// array retains 220 entries, but each slot now holds 600 declarations.
inline constexpr uint64_t SlotStride=0x70C0,Cursor=0x40;
inline constexpr uint64_t Declarations=0x48,DeclarationCount=0x70C8;
inline constexpr uint64_t UseCount=0x60E540,UseIds=0x60E548;
inline constexpr uint64_t AuxCount=0x616548,AuxKeys=0x61654C;
inline constexpr uint64_t BoundaryCount=0x61664C,BoundaryIds=0x616650;
inline constexpr uint32_t MaxSlots=220,MaxDeclarations=600;
inline constexpr uint32_t MaxUses=4096,MaxAuxKeys=64,MaxBoundaries=24;
static_assert(SlotStride*MaxSlots+Cursor==UseCount);
static_assert(Declarations+48*MaxDeclarations==DeclarationCount);
static_assert(UseIds+8*MaxUses==AuxCount);
static_assert(AuxKeys+4*MaxAuxKeys==BoundaryCount);
static_assert(BoundaryIds+8*MaxBoundaries==0x616710);
}
