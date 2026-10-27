#pragma once

// Haptics -- the controllers' vibration, one profile per thing the hand should feel.
//
// Callers on any thread ask for an EVENT; the XR frame thread plays it (Tick, after xrSyncActions),
// so xrApplyHapticFeedback only ever runs where the session's actions are synced. Each event is a
// short envelope of up to three segments (amplitude, milliseconds). Generic shots/swing requests
// restore the earlier fork's strength/length model; driving supplies lower-priority audio rumble.
// Interaction profiles, the master switch and the scale are exports
// (CyberpunkVR_HapticProfiles, CyberpunkVR_Haptics, CyberpunkVR_HapticScale); the last two are also
// ini keys (xr_haptics, xr_haptic_scale).

namespace cvr::haptics {

// Hand mask bits for Play.
inline constexpr int kLeft  = 1;
inline constexpr int kRight = 2;

// Event numbers, shared with the scripts (VRHapticEvent). Shots are not here: RecoilOnShot picks
// the generic recoil strength from WeaponKickDeg after validating the weapon class.
enum Event : int {
    kBeltTouch     = 1,    // a hand reached a tactical-belt element
    kGrenadePin    = 2,    // the pin came out -- the hand that pulled it
    kGrenadePinHold = 3,   // ...and the hand holding the grenade, weaker
    kMeleeBlock    = 10,   // a melee hit taken on the block
    kMeleeParry    = 11,   // a melee hit parried (deflect timing)
    kBulletBlock   = 12,   // a bullet stopped by the blade
    kBulletDeflect = 13,   // a bullet deflected or parried back
};

// One round left the barrel. weaponClass is CyberpunkVR_WeaponClass (1 handgun, 2 rifle/SMG/LMG,
// 3 shotgun, 4 sniper/precision); the trigger hand is the right one, and the left feels it too when
// the weapon is in a two-handed grip (CyberpunkVR_TwoHandActive), at the earlier fork's 0.55 share.
void Shot(int weaponClass, bool supportHandOnGun);
// Actual firing hand and that weapon's recoil stat; support is never copied in dual wield.
void ShotForHand(int hand, float kick, bool supportHandOnGun);

// A scripted event on the hands in handMask.
void Play(int event, int handMask);

// Generic per-hand swing/impact requests, merged until the XR frame consumes them.
void Pulse(int hand, float amplitude, int durationMs);
void SetVehicleHaptics(float left, float right);
void SetLegacyGain(float gain);
float LegacyGain();
void SetVehicleGain(float gain);
float VehicleGain();
// Discard on focus loss/failed sync/session teardown; never replay stale events.
void Clear();

// XR frame thread only: start whatever segment is due.
void Tick();

}  // namespace cvr::haptics
