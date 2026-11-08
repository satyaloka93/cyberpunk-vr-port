# Cyberpunk bridge polling hotfix

The user's installed bridge SHA256 is
`7d214ed4a46730fb5a574e40967437832b2a4c478a3340c322b5b6b865558c26`.
REA/Ghidra and instruction inspection confirm RVA 0xC3EA loads 250 into EAX;
the conditional TOW2 override loads 25, then the value is passed to setsockopt
(SOL_SOCKET, SO_RCVTIMEO) through the IAT at 0x14001C528. The direct Cyberpunk
configuration-file watcher shares this loop with blocking recvfrom. With CET's
UDP autostart disabled, there are no frequent packets to wake it.

Installed EDS uses Twitching, gearChangeDuration=16: CalcTimeIndex converts this
to roughly 160 ms of updates. A 250 ms idle wait can miss both the Machine shift
and its return to Resistance. The existing bike baseline swap and R2 shift patch
are intact; increasing force or changing sides does not fix a missed command.

The source correction is in `cyberpunk-polling-source.patch` for the later bridge
main.cpp: keep TOW2 at 25 ms and use 8 ms for Cyberpunk. It checks cleanly against
the user's current Toolkit source without modifying that dirty worktree.

The installed binary contains later code absent from the clean release worktree.
To avoid replacing it with an older build, `patch_polling.py` accepts only that
exact unsigned executable and the verified instruction sequence, changing just
the immediate byte 0xFA to 0x08 (file offset 47083). All other bytes, driver files,
trigger translations, full-game audio and motion settings stay unchanged.
Patched SHA256:
`a41626276b04d7f9740253341af644f3fd95447512e28a58b2f3b03927dd0b52`.
This is a local hotfix, not a newly rebuilt or published Toolkit release.

Run with an original input and a NEW output path:
`python patch_polling.py ORIGINAL.exe CANDIDATE.exe` (requires pefile).
It rejects unknown/already-patched/signed inputs and an existing output file.
Restart the bridge after backing up/replacing its executable; keep the launcher.
The phase test for a160ms effect misses90/250 old polling phases and none at8ms.
This establishes delivery opportunity, not physically verified trigger strength.

Suggested Toolkit PR: **Poll Cyberpunk trigger profiles faster than short gear effects**.
A future Toolkit source build should incorporate the source correction; do not
apply the binary patch to a different version.
