# Independent dual-wield aim and recoil on 0.2.2

Restore the firing-hand correction from RC5 `de19d872`, originally `3eec7518`.
0.2.2 already draws the left gun's dot, but a dot alone does not select the
projectile muzzle or recoil spring.

For hitscan, an exclusive logical firing hand takes priority over the attack's
weapon record: the game can report its primary weapon record for a left shot.
When both hands fire, matching distinct records still disambiguate; ambiguous
identical weapons retain the existing alternating fallback. Use 0.2.2's logical
firing-state bits, not RC5's physical trigger values, so handedness/remapping
continues to work.

For projectile providers, after the existing player-owner guard, select the
same hand for launch position, orientation and recoil. A left projectile uses
the published left muzzle transform, its own spread rotation and recoil latch.
The primary gun's sight-line offset never moves the left launch point. Pellet
spread is rotated around the selected muzzle, not discarded. The existing
40ms per-hand recoil coalescing remains intact.

The reload Lua module publishes the equipped weapon entities beside their
records. Native code compares those pointers against the already-used attack
participant slots; it does not traverse/dereference the weapon objects. Retain
0.2.2's deferred RTTI signature registration. Install the DLL and Lua update
together. Keep upstream's two-eye laser-dot renderer unchanged.

Tests compile and execute the production hitscan decision, projectile selection
and spread-rotation code. They cover primary/missing/unmatched records, either
or both triggers, identical weapons, inactive/invalid muzzle state, logical
versus physical trigger mismatch, position/orientation selection agreement,
and independent left/right spread. The Lua module is syntax checked.

Limitations: both-fire ambiguous attacks still need the existing fallback when
neither participant nor record identifies a gun. Dots are projected barrel
guides, not new impact raycasts. In-headset aim, recoil and physical haptics
remain a necessary final test.

Suggested PR: **Use the firing weapon for dual-wield projectile aim and recoil**.
Include the Lua publisher and native declaration/registration with the hooks.
This can be proposed independently of framegen and driving. The restored RC5
haptics commit improves the vibration produced by each selected recoil event;
it is not required for the aiming correction itself.
