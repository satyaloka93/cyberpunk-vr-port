#!/usr/bin/env python3
"""Analyse a Cyberpunk 2077 crash minidump without WinDbg.

Requires only the `minidump` package (pip install minidump). Intended for GPU-crash
reports, where CDPR's assert says the callstack is "probably irrelevant" -- the value
here is establishing what every thread was doing, and in particular whether any thread
was executing inside the VR port when the process died.

Usage:
    python3 scripts/analyze_crash_dump.py <path to Cyberpunk2077.dmp>

Note on method: a minidump carries no unwind information, so a true stack walk is not
possible. Scanning stack memory for values that land inside a loaded module produces
mostly noise -- the same address appears on many threads because it is a stored
callback or thread-entry pointer, not a live frame. This script therefore reports the
saved CPU context (RIP) per thread, which is exact.
"""
import collections
import sys

try:
    from minidump.minidumpfile import MinidumpFile
except ImportError:
    sys.exit("pip install minidump")

VR_MODULES = ("cyberpunkvr_stereo.dll", "cyberpunkvr_hands.dll")
GFX_HINTS = ("nvwgf2", "d3d12", "dxgi", "sl.", "nvngx", "reshade", "nvcuda", "amdxx")


def main(path):
    mf = MinidumpFile.parse(path)
    mods = sorted((m.baseaddress, m.baseaddress + m.size, m.name.split("\\")[-1])
                  for m in mf.modules.modules)

    def which(addr):
        for base, end, name in mods:
            if base <= addr < end:
                return name, addr - base
        return "?", 0

    rec = mf.exception.exception_records[0]
    crash_tid = rec.ThreadId
    exc = rec.ExceptionRecord
    name, off = which(exc.ExceptionAddress)

    print("== exception ==")
    print("  code      : %s" % exc.ExceptionCode)
    print("  address   : 0x%x -> %s+0x%x" % (exc.ExceptionAddress, name, off))
    print("  thread    : 0x%x (%d)" % (crash_tid, crash_tid))
    print("  threads   : %d" % len(mf.threads.threads))
    print("  modules   : %d" % len(mods))

    rows = []
    for t in mf.threads.threads:
        ctx = t.ContextObject
        rip = getattr(ctx, "Rip", None) if ctx else None
        if rip is not None:
            n, o = which(rip)
            rows.append((t.ThreadId, rip, n, o))

    print("\n== thread RIP distribution (exact, from saved context) ==")
    for n, c in collections.Counter(r[2] for r in rows).most_common():
        print("  %-30s %3d" % (n, c))

    print("\n== threads not parked in a wait stub ==")
    for tid, _, n, o in rows:
        if n in ("ntdll.dll", "KERNELBASE.dll", "win32u.dll"):
            continue
        mark = "   <-- ASSERTING" if tid == crash_tid else ""
        print("  tid 0x%-6x %-28s +0x%08x%s" % (tid, n, o, mark))

    print("\n== threads inside the VR port ==")
    vr = [r for r in rows if r[2].lower() in VR_MODULES]
    for tid, _, n, o in vr:
        print("  tid 0x%-6x %-28s +0x%08x" % (tid, n, o))
    if not vr:
        print("  none -- no thread was executing VR port code at dump time")

    print("\n== threads inside graphics modules ==")
    gfx = [r for r in rows if any(k in r[2].lower() for k in GFX_HINTS)]
    for tid, _, n, o in gfx:
        print("  tid 0x%-6x %-28s +0x%08x" % (tid, n, o))
    if not gfx:
        print("  none")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
