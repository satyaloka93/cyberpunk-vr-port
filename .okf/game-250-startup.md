---
type: Incident Analysis
title: Cyberpunk 2.50 startup failure before VR initialization
description: October26 startup evidence identifies obsolete CET and a native crash during ArchiveXL initialization, with separate SDK and fixed-hook incompatibilities in the current VR build.
tags: [compatibility, crash, red4ext, archivexl, cet, game-250]
timestamp: 2026-10-26
---

# Scope and result

**Follow-up:** after the user authorized repair, official prerequisites were
installed and the SDK migration built. A later search found current full-memory
dumps in `F:/crashdumps`, identifying the ArchiveXL callback ABI mismatch.
See [migration status](game-250-migration.md). The diagnosis below records the
initial read-only pass; its then-unavailable caller stack is now resolved.

User requested systematic diagnosis, not changes to the installed mod stack.
Read logs, Windows Application events and WER, inspected installed ArchiveXL
with REA/Ghidra, compared port source/SDK and two native byte guards, and checked
official compatibility releases. No game launch, injection, mod disable, runtime
configuration change, dependency install, source implementation or publication.
Local evidence copies and this knowledge record are the only writes.

**Immediate failure:** all five October26 attempts terminate during ArchiveXL
1.27.1 startup, before Codeware, VR, Input Loader, TweakXL and script compilation.
CET1.37.1 independently refuses the new game. The exact invalid call that causes
the native crash is not established without a retained dump/unwound caller stack.
ArchiveXL initialization is the strongly supported failure stage, not proof that
every other loaded component is faultless.

**Subsequent barrier:** the installed VR binary is still compiled against old
SDK object sizes/vtables and contains fixed2.31 RVAs. Updating loader DLLs alone
does not repair those assumptions. Do not claim a dependency update alone will
restore VR, or bypass version checks to force compatibility.

# Identity and timeline

Game path: D:/SteamLibrary/steamapps/common/Cyberpunk 2077.
New product2.50, file3.0.88.46235. EXE SHA256
d36e74c47592e9c656ae250078cb40b2be63c782af5bfbeff2280d43e653fab4.
October21 baseline was product2.31/file3.0.80.51928.

Five attempts:10:26:29,10:26:47,10:37:46,10:38:29,10:39:10KST.
Latest RED4ext log red4ext/logs/red4ext-2026-10-26-10-39-10.log:

* 10:39:10.701: reports RED4ext1.30.0 and game2.50.
* 10:39:11.731: loads1,032,136 addresses from game's current address JSON.
* 10:39:11.737: initialization succeeds.
* 10:39:11.774: starts loading ArchiveXL.dll; no loaded-success entry follows.
* ArchiveXL-2026-10-26-10-39-11.log contains only its1.27.1 starting line
  at10:39:11.779.
* Windows Event1000 records C0000005 at0x00000241AB300140 at10:39:11;
  Event1001 records BEX64 / StackHash_89de with exception data8.

Latest bin/x64/cyberpunkvrport.log timestamp remains October21 14:36:28 and
ends with orderly VR shutdown/unload. The later October21 process shutdown
crash is historical, not evidence of the new startup failure. Current WER's
loaded-module list contains RED4ext, CET, ArchiveXL, Cheeky host/runtime, but
not CyberpunkVR_Stereo, Codeware or TweakXL.

CET logs each October26 start asv1.37.1 and reports:
`Unsupported game version! Only 2.31 is supported.`
This establishes a separate compatibility blocker, not its responsibility for
the native execute-access violation.

redscript_rCURRENT.log is still October21 14:27:20, successful compilation.
Codeware/TweakXL/InputLoader logs also remain October21. ReShade.log is September27
and irrelevant to the current run. Cheeky host and standalone logs DO update
October26; they are not stale. Last host line is initializing; standalone
registers its exception handler and installs Streamline hooks on another thread.

# Exception evidence and limits

C:/Users/gthom/AppData/Local/Temp/CheekyFoveatedDLSS_crash.log captured all five
new failures. Latest tid20744 matches RED4ext/ArchiveXL's startup thread, while
Cheeky's initialization/hooking thread is11036. Recorded RIP/target
0x00000241AB300140 exactly matches Event1000. Memory operation8 is execute;
VirtualQuery protection0x04 is read/write, non-executable, state0x1000 committed.
Pending DLL notification includes ArchiveXL.dll base0x00007FFBC56B0000.

Other failures have relocated heap targets ending0140 and the same Windows
bucket. This is repeatable bad control flow, not a graphics frame-time problem.
Cheeky appearing in the exception-handler stack is not proof it caused it.
The stack stops at the invalid target; no source caller is recoverable from it.
Do not assign a specific SDK member or trampoline as the culprit solely from
this evidence. Concurrent hooks remain a bounded uncertainty until isolation
or a proper crash stack, but obsolete dependencies must be addressed regardless.

Latest WER directory:
C:/ProgramData/Microsoft/Windows/WER/ReportArchive/
AppCrash_Cyberpunk2077.ex_c7d31f89b7bd1a32b30dbea5f7c5b318bcd1a7_dd0cc7db_0678fba1-46e8-4922-85ad-deea9b5a9be7.
Only Report.wer remains. Named WER temp dump is gone; local CrashDumps has no
Cyberpunk dump. REDEngine ReportQueue newest isOctober21, so do not analyze it
as this incident. No new runtime was launched to manufacture a dump.

# REA evidence

Inspected installed ArchiveXL.dll SHA256
cee7aaccfeaa19ae2f865bb80e08afdd63fa1f14968c44c0a0538bf0b8d931b2,
x64PE; Ghidra12.1.4. Main0x180135200 creates the plugin object then invokes
startup callback iteration. The starting-log string0x180176770 is referenced
in0x18003f600, before remaining plugin callbacks. This corroborates how early
the single log line is; static analysis does not establish which callback ran
at the fault. Analysis session closed without binary/database modification.

* Main pseudocode: ev_9082d9d0549cd58399b0825576fa96385b23d6ffb485c66af151741c176979d2.
* Startup dispatcher: ev_56a4510fda1e0ade6cdcc000ea09980b2a137afa81295d6b2c5775838878ff3b.
* Starting-log function: ev_cce630c1de4a35b0ac9ed6ab4b6e23774f579447196d9a6c7ece57613d43317c.

Complete ten-record bundle in parent repo:
build/diagnostics/game250/archivexl-rea-evidence-20261026.json.
Hash-verified log copies, WER and Application event JSON:
build/diagnostics/game250/snapshot-20261026-104449-783.
Collectors are read-only with respect to game files.

# Installed dependencies and official fixes

| Component | Installed evidence | Official 2.50 release |
|---|---|---|
| RED4ext | logs/FileVersion1.30.0 | [1.31.0](https://github.com/wopss/RED4ext/releases/tag/v1.31.0) |
| CET | runtime log1.37.1, explicit rejection | [1.38.1](https://github.com/maximegmd/CyberEngineTweaks/releases/tag/v1.38.1); [1.38.0](https://github.com/maximegmd/CyberEngineTweaks/releases/tag/v1.38.0) adds3.0.88.46235 |
| ArchiveXL | log1.27.1/file1.27.1.2607291119 | [1.28.0](https://github.com/psiberx/cp2077-archive-xl/releases/tag/v1.28.0) |
| Codeware | file1.20.3.2605040034; not reached today | [1.21.1](https://github.com/psiberx/cp2077-codeware/releases/tag/v1.21.1) |
| TweakXL | file1.11.4.2607282033; not reached today | [1.12.0](https://github.com/psiberx/cp2077-tweak-xl/releases/tag/v1.12.0) |
| redscript | switcher pins0.5.31; installed tools dateAug28,2025, no version resource; compiler not reached | [0.5.32](https://github.com/jac3km4/redscript/releases/tag/v0.5.32) |

RED4ext SHA25659309a78a9606481d13f87e8fbf9ea9c0b735115389ded0295537180c9e52511.
Version1.30.0 alone does not distinguish an original release from a patched
build retaining the same version; exact published-archive provenance not verified.
There is no need to substitute unofficial builds now that official fixes exist.
InputLoader runtime metadata previously said0.1.1 while package manifest says0.2.3;
do not use that mismatch alone as proof of the installed package version.

# Our native port's separate compatibility problem

Installed DLL hashcf6d6986f255a9d108673132e375559d14c4807b48aca8a6c312a9f94c5c3f28
matches the [October21 haptics deployment](generic-openxr-haptics.md). It has not
silently been replaced. Main.cpp still declares runtime independent, with stale
embedded version0.1.7 despite its upstream0.1.8 source; the hash is the identity.

Its SDK submodule isd02a1f329744cd4e39c6f7f3fdf63053731e1aff. Current local headers:
CBaseFunction0xB0; CGlobalFunction0xB8; CStackFrame0x68; CGameEngine.framework0x308.
Official [SDK PR227](https://github.com/wopss/RED4ext.SDK/pull/227) changes these
to0xF0,0xF8,0xA8,0x348 respectively and adds RTTI virtuals. [PR230](https://github.com/wopss/RED4ext.SDK/pull/230)
corrects transaction virtual slots; [PR232](https://github.com/wopss/RED4ext.SDK/pull/232)
regenerates native types. Our native registration constructs these functions,
and our SDK accesses the engine framework at the old offset. Replacing the
loader cannot alter compiled structure sizes or call sites in this DLL.

Read-only check_hook_guards.py maps PE RVAs to file offsets before comparison.
Both BodyHeading.cpp exact2.31 guards fail on installed2.50:

| RVA | Expected prefix | Actual prefix |
|---|---|---|
| 0x57B714 | 48895c240848897c241055488bec4883ec70 | 204889080f114010eb1b0f104720488d5570 |
| 0xD455B0 | 40534883ec300f2805c3b84a02498bd80f1102f30f1089a4000000 | 488b4a40ff14c6488b034c8d4424384883642438004533c9488363 |

Those two hooks refuse installation on mismatch. Other core stereo detours are
registered as fixedRVAs; DetourRegistry.cpp computes base+rva and directly calls
MinHook without a universal byte-identity guard. A successful MinHook return
does not prove it hooked the intended function after a game update.
Only two guards were byte-tested; this is not a complete2.50 hook audit.

Upstream latest release is still0.1.8/Oct19 and main9aa2601 at investigation time.
[Issue113](https://github.com/dariulone/cyberpunk-vr-port/issues/113) reports early
startup failure on2.5; no upstream2.50 fix was found in current main/releases.

# Repair order (not executed)

1. Preserve current settings/mods, update the six prerequisites from official
   releases as a coherent set. Keep saves, bindings, haptics and graphics tuning.
2. Port against the2.50 SDK and audit/re-resolve engine detours and signatures;
   add a supported-build guard rather than advertise arbitrary compatibility.
3. Verify initialization reaches all plugins and compiles scripts, then verify
   VR rendering/input with our haptics/grip/pacing changes intact. Capture a
   contemporary dump if startup still fails; don't reuse October21 dumps.

No claim of restored startup or physical VR verification has been made.
