// Temporary event-level 2.50 equip trace; never changes results or inventory.
public native func VRWeaponTrace(message: String) -> Void;

@wrapMethod(EquipmentSystemPlayerData)
public final func GetItemIDfromEquipmentManipulationAction(action: EquipmentManipulationAction, currentState: SSlotActiveItems) -> SSlotActiveItems {
  let result = wrappedMethod(action, currentState);
  VRWeaponTrace("resolve action=" + ToString(action) + " right=" + ToString(ItemID.GetTDBID(result.rightHandItem)) + " left=" + ToString(ItemID.GetTDBID(result.leftHandItem)));
  return result;
}

@wrapMethod(EquipmentBaseTransition)
protected const func HandleWeaponEquip(scriptInterface: ref<StateGameScriptInterface>, stateContext: ref<StateContext>, stateMachineInstanceData: StateMachineInstanceData, item: ItemID, isLeftWeapon: Bool) -> Void {
  wrappedMethod(scriptInterface, stateContext, stateMachineInstanceData, item, isLeftWeapon);
  if IsDefined(scriptInterface.executionOwner as PlayerPuppet) {
    VRWeaponTrace("equip item=" + ToString(ItemID.GetTDBID(item)) + " left=" + ToString(isLeftWeapon));
  };
}

@wrapMethod(EquipmentBaseTransition)
protected final const func HandleWeaponUnequip(scriptInterface: ref<StateGameScriptInterface>, stateContext: ref<StateContext>, stateMachineInstanceData: StateMachineInstanceData, item: ItemID, isLeftWeapon: Bool) -> Void {
  if IsDefined(scriptInterface.executionOwner as PlayerPuppet) {
    VRWeaponTrace("unequip item=" + ToString(ItemID.GetTDBID(item)) + " left=" + ToString(isLeftWeapon));
  };
  wrappedMethod(scriptInterface, stateContext, stateMachineInstanceData, item, isLeftWeapon);
}

@wrapMethod(UpperBodyEventsTransition)
protected final func UpdateSwitchItem(timeDelta: Float, stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Bool {
  let next: Bool = scriptInterface.IsActionJustPressed(n"NextWeapon");
  let previous: Bool = scriptInterface.IsActionJustPressed(n"PreviousWeapon");
  let switch: Bool = scriptInterface.IsActionJustTapped(n"SwitchItem", 0.35);
  let holster: Bool = scriptInterface.IsActionJustTapped(n"HolsterWeapon");
  if next || previous || switch || holster {
    VRWeaponTrace("input next=" + ToString(next) + " previous=" + ToString(previous) + " switch=" + ToString(switch) + " holster=" + ToString(holster));
  };
  return wrappedMethod(timeDelta, stateContext, scriptInterface);
}

@wrapMethod(ReadyEvents)
protected final func OnEnter(stateContext: ref<StateContext>, scriptInterface: ref<StateGameScriptInterface>) -> Void {
  wrappedMethod(stateContext, scriptInterface);
  let weapon = this.GetWeaponObject(scriptInterface);
  if IsDefined(scriptInterface.executionOwner as PlayerPuppet) && IsDefined(weapon) && Equals(weapon.GetWeaponRecord().ItemType().Type(), gamedataItemType.Wea_ShotgunDual) {
    VRWeaponTrace("shotgun-ready item=" + ToString(ItemID.GetTDBID(weapon.GetItemID())) + " right=" + ToString(this.IsRightWeaponLogic()));
  };
}
