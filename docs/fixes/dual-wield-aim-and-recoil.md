# Dual-wield aiming, sight dots and recoil routing

Left-hand fire could follow the right-hand sight dot and animate right-hand recoil.
Hitscan selection now prioritizes an exclusively pressed trigger over a shared
primary weapon record. Projectile launch position, orientation and recoil now
select the same hand, using left muzzle data for left shots and independent cone
rotation state for simultaneous shots.

CET publishes equipped weapon handles through SetVRDualWieldEntities. When trigger
state is ambiguous, existing owner-checked attack participant fields are compared
with these addresses. Weapon objects are not dereferenced by the new classifier.
Unresolved simultaneous/identical-weapon cases retain existing fallback behavior;
this is not a guarantee of perfect identity for every attack type.

The cyan left sight dot and red right dot are drawn in both SBS eye viewports.
They are projected barrel guides at the configured range, not impact raycasts.

Deploy the native DLL and CyberpunkVRPort_Reload/dual_wield.lua together.
Validation: test_dual_shot_selection.py and test_dual_projectile.py compile actual
production selection/rotation blocks, exercise single/both triggers and known,
missing and identical weapon identities, and check Lua syntax. MSVC Release passed.
Specific live confirmation of every dual-weapon combination remains outstanding.
