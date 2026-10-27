public native func VRWeaponShot(hand: Int32, kick: Float, dual: Bool) -> Void;
public native func VRWeaponShotCounts() -> Vector4;
public native func VRRightWeaponShot() -> Void;
public native func VRWeaponTriggerEnabled() -> Bool;

@addField(WeaponObject) private let vrTriggerHasShot: Bool;
@addField(WeaponObject) private let vrTriggerLastShotReal: Float;

@addMethod(WeaponTransition)
protected final const func VRDirectSemiAuto(scriptInterface: ref<StateGameScriptInterface>) -> Bool {
  // Native automatic cadence, burst spacing, charge, reload, ammo and weapon
  // safety remain game-owned. Single-shot repeat delay uses a uniform cap.
  return VRWeaponTriggerEnabled() && IsDefined(scriptInterface.executionOwner as PlayerPuppet) &&
    scriptInterface.IsTriggerModeActive(gamedataTriggerMode.SemiAuto);
}

@wrapMethod(WeaponTransition)
protected final const func CanPerformNextShotInSequence(const weaponObject: ref<WeaponObject>, const stateContext: ref<StateContext>, const scriptInterface: ref<StateGameScriptInterface>) -> Bool {
  if this.VRDirectSemiAuto(scriptInterface) {
    // Five deliberate pulls per real second. Use the physical-input clock so
    // slow motion does not turn a 200ms reset into a multi-second wait.
    let pullReset: Bool = !weaponObject.vrTriggerHasShot || VRMeleeRealTime() - weaponObject.vrTriggerLastShotReal >= 0.20;
    // SNIPER RIFLES KEEP THEIR OWN CYCLE (2026-10-16). The uniform 200ms
    // reset let a bolt gun fire five rounds a second; on top of it the weapon's
    // own CycleTime gate applies again, exactly as in the flat game.
    if Equals(weaponObject.GetWeaponRecord().ItemType().Type(), gamedataItemType.Wea_SniperRifle) {
      return pullReset && wrappedMethod(weaponObject, stateContext, scriptInterface);
    };
    return pullReset;
  };
  return wrappedMethod(weaponObject, stateContext, scriptInterface);
}

@wrapMethod(ShootEvents)
protected final func OnEnter(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  let weapon = this.GetWeaponObject(scriptInterface);
  if this.VRDirectSemiAuto(scriptInterface) && IsDefined(weapon) {
    // Start the interval on the native shot, never on a blocked trigger pull.
    weapon.vrTriggerLastShotReal = VRMeleeRealTime();
    weapon.vrTriggerHasShot = true;
  };
  wrappedMethod(stateContext, scriptInterface);
  if IsDefined(scriptInterface.executionOwner as PlayerPuppet) {
    if IsDefined(weapon) && weapon.IsRanged() {
      let otherSlot: TweakDBID = this.IsRightWeaponLogic() ? t"AttachmentSlots.WeaponLeft" : t"AttachmentSlots.WeaponRight";
      let other = scriptInterface.GetTransactionSystem().GetItemInSlot(scriptInterface.executionOwner, otherSlot) as WeaponObject;
      let dual: Bool = IsDefined(other) && other.IsRanged();
      let kick = scriptInterface.GetStatsSystem().GetStatValue(Cast<StatsObjectID>(weapon.GetEntityID()), gamedataStatType.RecoilKickMax);
      VRWeaponShot(this.IsRightWeaponLogic() ? 1 : 0, kick, dual);
    };
    if this.IsRightWeaponLogic() { VRRightWeaponShot(); };
  };
}

@wrapMethod(WeaponTransition)
protected final const func IsSemiAutoAction(weaponObject: ref<WeaponObject>, const stateContext: ref<StateContext>, const scriptInterface: ref<StateGameScriptInterface>) -> Bool {
  let lastPress: StateResultInt;
  if this.VRDirectSemiAuto(scriptInterface) {
    // 2.50 owns hand selection and records a separate shot count per hand.
    if !this.ShouldShoot(scriptInterface) { return false; };
    lastPress = stateContext.GetPermanentIntParameter(this.GetLastChargePressCount());
    // ShootEvents records this native count on every actual shot. The count
    // survives a one-frame Shoot/CycleRound transition, unlike JustPressed,
    // while holding at .84 after the break cannot create another shot.
    return scriptInterface.GetActionValue(n"RangedAttack") > 0.00 &&
      (!lastPress.valid || lastPress.value != Cast<Int32>(scriptInterface.GetActionPressCount(n"RangedAttack")));
  };
  return wrappedMethod(weaponObject, stateContext, scriptInterface);
}

@wrapMethod(WeaponTransition)
protected final func StartShootingSequence(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>, fireDelay: Float, burstCycleTime: Float, numShotsBurst: Int32, isFullChargeFullAuto: Bool) -> Void {
  wrappedMethod(stateContext, scriptInterface, this.VRDirectSemiAuto(scriptInterface) ? 0.00 : fireDelay, burstCycleTime, numShotsBurst, isFullChargeFullAuto);
}
