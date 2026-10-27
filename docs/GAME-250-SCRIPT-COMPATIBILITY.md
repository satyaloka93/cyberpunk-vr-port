# Game 2.50 script compatibility

The first migrated native build loaded with ArchiveXL, Codeware, Input Loader
and TweakXL on October 26. Script compilation then failed before gameplay.
The actual game bundle was decompiled with the official redscript CLI v0.5.32
to establish the new signatures, rather than guessing from error messages.

## Port source changes

* `EquipmentBaseTransition.HandleWeaponEquip` gained `isLeftWeapon: Bool`.
  Both weapon-animation wrappers now forward that argument. Original author
  attribution in ControlLowerAndHolsterWeapons is retained.
* `EquipmentSystemPlayerData.GetItemIDfromEquipmentManipulationAction` now
  receives and returns `SSlotActiveItems`, containing both hands. The launcher
  wrapper forwards the current state and retains native slot assignment and
  secondary-weapon handling. Its wheel action routes through native
  `CycleNextWeaponWheelItem`, which calls the existing VR cycle wrapper.
* The ADS pose bridge reads `PlayerStateMachine.RightWeapon`, replacing the
  removed `Weapon` field. This remains the right-hand pose measurement.
* Physical optics obtains the equipped weapon through
  `GameObject.GetActiveWeapon(scriptInterface.executionOwner)`; the removed
  `DefaultTransition.GetActiveWeapon` helper is no longer referenced.

## Local third-party compatibility edits

These changes were applied to the user's installed scripts with backups;
their full third-party sources are not added to this repository.

* LUTSwitcher `LUTCustomize.reds`: use the compiler's generic `Equals` for
  ColorGradingAreaSettings references. The overloaded `==` accepts IScriptable
  references, whereas the new native settings inheritance is ISerializable.
* MoreOccluders: retain the existing ordinary `worldMeshNode` optimization.
  Skip the old instanced-mesh branch: `worldInstancedMeshNode` is now a
  `worldCookedNode` with no exposed mesh appearance or occluder fields.
  This is a reduced optimization scope, not equivalent instanced-mesh support.
  Do not invent offsets or modify all instanced occluders without the old
  transparent-material eligibility check.

## Verification and deployment

All installed script mods plus ArchiveXL/Codeware/TweakXL scripts were copied
to an isolated tree and compiled against a copy of the installed vanilla2.50
`final.redscripts`, using the game's installed `scc.exe`/`scc_lib.dll`0.5.32.
The complete compile succeeded at15:07:20KST, October26. No mods were removed
from the compile set. The standalone redscript CLI uses stricter temporary
expression diagnostics than the game SCC path; its earlier failure is retained
but is not the game-mode acceptance result. Unrelated third-party scripts were
not rewritten merely to satisfy that standalone policy.

Seven changed source files were deployed and hash-verified. The compiled
offline cache was **not** copied over the game cache. The next launch compiles
the installed files normally. Native DLL, user settings and other mods remain
unchanged by this script repair. Runtime launcher/weapon/optics behavior still
needs a game test; compiler success alone does not establish it.

Parent-repository evidence:

* `build/diagnostics/game250/scripts/offline/r6/logs/redscript_rCURRENT.log`
* `build/diagnostics/game250/scripts/backups/20261026-150755-390/receipt.json`
* `build/diagnostics/game250/scripts/prepare_fixes.py`
* `build/diagnostics/game250/scripts/Deploy-ScriptFixes250.ps1`
* `build/diagnostics/game250/scripts/vanilla250.reds` (local research only)

The first native log also reports a missing KeypadCursor hook. That is a
separate UI compatibility item, not the cause of script compilation failure.
