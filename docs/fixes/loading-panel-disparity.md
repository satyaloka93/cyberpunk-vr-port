# End-of-load panel rendered mono until dismissed

The shader-disparity candidate deployed November3 19:00 did not remove the
reported left strip. Its helper and shader-camera rewrite have been removed.
Retained screenshots show source-artwork displacement and a clipped prompt;
the precise drawing-stage cause was not established by the failed candidate.

The new correction holds the engine SDK availability false while the recognized
loading layer is active. Closing the frontend arms SBS, but does not override
this loading gate. The game renders the loading screen once across the full
window; existing full-window capture presents it on the headset panel. SBS
resumes when the layer is dismissed. The arming latch is retained; subsequent
save loads use the same behavior. No lobby is imported and gameplay settings,
projection, framegen backend and HUD depth remain unchanged.

Verified against the installed game binary: 0x14227e858 calls wrapper
0x142284dc0, which invokes interface vtable+0x50 (Available). Its false result
branches past setting feature98 at0x14227edb5. Native disassembly evidence is in
the parent build/loading-panel-review-20261103/mono-gate directory. REA's
procedure requests did not return promptly; focused local PE disassembly was
used for this verification, not invented decompiler output.

Tests compile the actual SDK gate for initial frontend, armed-but-loading,
continuation and repeated loads; existing frontend lifetime and panel InkShift
checks also pass. A bounded log reports mono frontend / mono loading panel /
SBS gameplay, capped16 transitions per process. User confirmed the loading-panel strip is fixed on November 3 after deployment.
