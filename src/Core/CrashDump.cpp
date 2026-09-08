#include "Core/CrashDump.hpp"

#include <windows.h>
#include <dbghelp.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <share.h>

#include <MinHook.h>

extern void Log(const char* fmt, ...);

namespace {

// ---------------------------------------------------------------------------
// Configuration -- vrport.ini, flat key=value like the rest of that file.
//
//   crash_dump=1               master switch
//   crash_dump_dir=            empty -> beside cyberpunkvrport.log (bin\x64)
//   crash_dump_full=0          0 = stacks + referenced memory, 1 = full memory
//   crash_dump_max=3           dumps written per session
//   crash_dump_first_chance=0  also dump on a FIRST-CHANCE fatal exception
//
// first_chance defaults off on purpose. The vectored handler sees every fault
// before anything gets to handle it, and CP2077 raises faults it recovers from;
// dumping on those would bury the one that matters. Logging them is free, so
// that happens always -- the log line alone answers the question this was built
// for, which is whether an exception is raised at all.
// ---------------------------------------------------------------------------
struct Config {
    bool enabled{true};
    bool fullMemory{false};
    bool dumpFirstChance{false};
    bool continueBreakpoint{true};
    int  maxDumps{3};
    char directory[MAX_PATH]{};
};

Config g_config;
std::atomic<int>  g_dumpsWritten{0};
std::atomic<int>  g_eventsHandled{0};
std::atomic<bool> g_installed{false};
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

// A fault that is never legitimately recovered from. Anything outside this set
// -- C++ throws (0xE06D7363), the debugger's 0x406D1388 thread-name ping, the
// probes the loader raises -- is left alone entirely.
bool IsFatalCode(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case 0xC0000409:  // STATUS_STACK_BUFFER_OVERRUN / __fastfail
        return true;
    default:
        return false;
    }
}

const char* CodeName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
    case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLT_DIVIDE_BY_ZERO";
    case 0xC0000409:                      return "STACK_BUFFER_OVERRUN/fastfail";
    default:                              return "?";
    }
}

void GameDirectory(char* out, size_t size) {
    out[0] = '\0';
    char path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0) return;
    char* lastSlash = strrchr(path, '\\');
    if (!lastSlash) return;
    *(lastSlash + 1) = '\0';
    strncpy_s(out, size, path, _TRUNCATE);
}

void LoadConfig() {
    GameDirectory(g_config.directory, MAX_PATH);

    char iniPath[MAX_PATH];
    GameDirectory(iniPath, MAX_PATH);
    strncat_s(iniPath, MAX_PATH, "vrport.ini", _TRUNCATE);

    FILE* file = _fsopen(iniPath, "r", _SH_DENYNO);
    if (!file) return;

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char* equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';
        const char* key = line;
        char* value = equals + 1;

        // Trim trailing newline and surrounding blanks from the value.
        size_t len = strlen(value);
        while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r' ||
                           value[len - 1] == ' '  || value[len - 1] == '\t')) {
            value[--len] = '\0';
        }
        while (*value == ' ' || *value == '\t') ++value;

        if (_stricmp(key, "crash_dump") == 0) {
            g_config.enabled = atoi(value) != 0;
        } else if (_stricmp(key, "crash_dump_full") == 0) {
            g_config.fullMemory = atoi(value) != 0;
        } else if (_stricmp(key, "crash_dump_first_chance") == 0) {
            g_config.dumpFirstChance = atoi(value) != 0;
        } else if (_stricmp(key, "crash_dump_continue_breakpoint") == 0) {
            g_config.continueBreakpoint = atoi(value) != 0;
        } else if (_stricmp(key, "crash_dump_max") == 0) {
            const int parsed = atoi(value);
            if (parsed >= 0) g_config.maxDumps = parsed;
        } else if (_stricmp(key, "crash_dump_dir") == 0 && *value != '\0') {
            strncpy_s(g_config.directory, MAX_PATH, value, _TRUNCATE);
            const size_t dirLen = strlen(g_config.directory);
            if (dirLen > 0 && g_config.directory[dirLen - 1] != '\\') {
                strncat_s(g_config.directory, MAX_PATH, "\\", _TRUNCATE);
            }
        }
    }
    fclose(file);
}

// The module that owns an address, plus the offset into it. This is the single
// most useful line in the whole file: "faulting module + RVA" is what turns a
// bare address into something you can look up in a .pdb or a map file.
void DescribeAddress(void* address, char* out, size_t size) {
    out[0] = '\0';
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(address),
            &module) ||
        module == nullptr) {
        _snprintf_s(out, size, _TRUNCATE, "<unmapped>");
        return;
    }

    char path[MAX_PATH];
    if (GetModuleFileNameA(module, path, MAX_PATH) == 0) {
        _snprintf_s(out, size, _TRUNCATE, "<module %p>", static_cast<void*>(module));
        return;
    }
    const char* name = strrchr(path, '\\');
    name = name ? name + 1 : path;

    const auto rva = static_cast<unsigned long long>(
        reinterpret_cast<const unsigned char*>(address) -
        reinterpret_cast<const unsigned char*>(module));
    _snprintf_s(out, size, _TRUNCATE, "%s+0x%llX (base %p)", name, rva,
                static_cast<void*>(module));
}

// Who called us, named. Without this an exit hook says only "something exited";
// with it the frames name the module and offset that decided to.
void LogCallStack(const char* label) {
    void* frames[24]{};
    const USHORT captured = RtlCaptureStackBackTrace(1, 24, frames, nullptr);
    Log("[CRASH] %s -- %u frame(s):\n", label, static_cast<unsigned>(captured));
    for (USHORT index = 0; index < captured; ++index) {
        char site[MAX_PATH + 64];
        DescribeAddress(frames[index], site, sizeof(site));
        Log("[CRASH]   #%02u %p  %s\n", static_cast<unsigned>(index), frames[index], site);
    }
}

struct FaultRequest {
    EXCEPTION_POINTERS* pointers;
    DWORD threadId;
    bool  writeDump;
    bool  unhandled;
};

// Everything runs on a fresh thread with a fresh stack. On EXCEPTION_STACK_OVERFLOW
// the faulting thread has a page or two left, which is not enough for dbghelp and
// not reliably enough for the CRT -- and that case is exactly the one worth
// capturing, so it cannot be the one that fails.
DWORD WINAPI FaultThread(LPVOID parameter) {
    auto* request = static_cast<FaultRequest*>(parameter);
    const EXCEPTION_RECORD* record = request->pointers->ExceptionRecord;

    char site[MAX_PATH + 64];
    DescribeAddress(record->ExceptionAddress, site, sizeof(site));

    Log("[CRASH] %s code=0x%08X at %p -> %s | thread=%lu | %s\n",
        CodeName(record->ExceptionCode),
        static_cast<unsigned>(record->ExceptionCode),
        record->ExceptionAddress,
        site,
        request->threadId,
        request->unhandled ? "UNHANDLED" : "first-chance");

    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
        if (record->NumberParameters >= 2) {
            const ULONG_PTR operation = record->ExceptionInformation[0];
            const auto target = reinterpret_cast<void*>(record->ExceptionInformation[1]);
            char targetSite[MAX_PATH + 64];
            DescribeAddress(target, targetSite, sizeof(targetSite));
            Log("[CRASH]   %s address %p -> %s\n",
                operation == 0 ? "read from" : operation == 1 ? "write to" : "execute at",
                target, targetSite);
        }
    }

    if (!request->writeDump) return 0;

    HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
    if (!dbghelp) {
        Log("[CRASH]   dbghelp.dll unavailable; no dump written\n");
        return 0;
    }
    using MiniDumpWriteDumpFn = BOOL(WINAPI*)(
        HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
        PMINIDUMP_EXCEPTION_INFORMATION,
        PMINIDUMP_USER_STREAM_INFORMATION,
        PMINIDUMP_CALLBACK_INFORMATION);
    const auto writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(
        GetProcAddress(dbghelp, "MiniDumpWriteDump"));
    if (!writeDump) {
        Log("[CRASH]   MiniDumpWriteDump missing; no dump written\n");
        return 0;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);
    char dumpPath[MAX_PATH];
    _snprintf_s(dumpPath, sizeof(dumpPath), _TRUNCATE,
                "%sCyberpunkVRPort-crash-%04u%02u%02u-%02u%02u%02u-pid%lu.dmp",
                g_config.directory, now.wYear, now.wMonth, now.wDay,
                now.wHour, now.wMinute, now.wSecond, GetCurrentProcessId());

    HANDLE file = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        Log("[CRASH]   could not create %s (err=%lu)\n", dumpPath, GetLastError());
        return 0;
    }

    MINIDUMP_EXCEPTION_INFORMATION info{};
    info.ThreadId = request->threadId;
    info.ExceptionPointers = request->pointers;
    info.ClientPointers = FALSE;  // same process

    // Default is deliberately not MiniDumpWithFullMemory: a full CP2077 dump is
    // 12 GB. Indirectly-referenced memory keeps every thread stack and what those
    // stacks point at, which is what a fault site needs.
    const MINIDUMP_TYPE type = g_config.fullMemory
        ? static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory |
                                     MiniDumpWithHandleData |
                                     MiniDumpWithThreadInfo |
                                     MiniDumpWithUnloadedModules)
        : static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
                                     MiniDumpWithDataSegs |
                                     MiniDumpWithThreadInfo |
                                     MiniDumpWithUnloadedModules);

    const BOOL ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(),
                              file, type, &info, nullptr, nullptr);
    const DWORD error = ok ? 0 : GetLastError();
    const LARGE_INTEGER size = [file]() {
        LARGE_INTEGER value{};
        GetFileSizeEx(file, &value);
        return value;
    }();
    CloseHandle(file);

    if (ok) {
        Log("[CRASH]   dump written: %s (%.1f MB)\n",
            dumpPath, static_cast<double>(size.QuadPart) / (1024.0 * 1024.0));
    } else {
        Log("[CRASH]   MiniDumpWriteDump failed (err=0x%08X)\n",
            static_cast<unsigned>(error));
    }
    return 0;
}

void ReportFault(EXCEPTION_POINTERS* pointers, bool unhandled) {
    bool writeDump = unhandled || g_config.dumpFirstChance;
    if (writeDump) {
        // Reserve a slot up front so two threads faulting at once cannot both
        // pass a check that only one of them should.
        int written = g_dumpsWritten.load(std::memory_order_relaxed);
        for (;;) {
            if (written >= g_config.maxDumps) { writeDump = false; break; }
            if (g_dumpsWritten.compare_exchange_weak(written, written + 1,
                                                     std::memory_order_acq_rel)) {
                break;
            }
        }
    }

    FaultRequest request{pointers, GetCurrentThreadId(), writeDump, unhandled};
    HANDLE thread = CreateThread(nullptr, 0, FaultThread, &request, 0, nullptr);
    if (!thread) {
        // No thread to borrow a stack from; the inline attempt is better than
        // losing the record entirely.
        FaultThread(&request);
        return;
    }
    // `request` lives on this stack, so the fault thread must finish with it
    // before we return. It is bounded work and this thread is already dying.
    WaitForSingleObject(thread, 30000);
    CloseHandle(thread);
}

// Every DISTINCT exception code, once. The first pass of this handler only
// logged the fatal set and the log came back with nothing in it, which proves
// only that no fatal-class fault was raised -- not that none was. One line per
// code, capped, settles that for good at the cost of a linear scan over a
// handful of DWORDs.
constexpr int kMaxDistinctCodes = 24;
std::atomic<DWORD> g_seenCodes[kMaxDistinctCodes]{};

bool NoteDistinctCode(DWORD code) {
    for (int index = 0; index < kMaxDistinctCodes; ++index) {
        DWORD seen = g_seenCodes[index].load(std::memory_order_acquire);
        if (seen == code) return false;
        if (seen == 0) {
            DWORD expected = 0;
            if (g_seenCodes[index].compare_exchange_strong(expected, code,
                                                           std::memory_order_acq_rel)) {
                return true;
            }
            if (g_seenCodes[index].load(std::memory_order_acquire) == code) return false;
        }
    }
    return false;
}

LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* pointers) {
    if (!pointers || !pointers->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = pointers->ExceptionRecord->ExceptionCode;

    if (NoteDistinctCode(code)) {
        char site[MAX_PATH + 64];
        DescribeAddress(pointers->ExceptionRecord->ExceptionAddress, site, sizeof(site));
        Log("[CRASH] first exception of code 0x%08X (%s) at %p -> %s | thread=%lu\n",
            static_cast<unsigned>(code), CodeName(code),
            pointers->ExceptionRecord->ExceptionAddress, site, GetCurrentThreadId());
    }

    // ---------------------------------------------------------------------
    // STATUS_BREAKPOINT -- an int 3 someone compiled in, not a fault.
    //
    // This is what has been ending the process. Nothing HANDLES a breakpoint
    // when no debugger is attached, so the default disposition kills the game;
    // attach any debugger and it is absorbed and execution carries on. That is
    // the whole of "it works under procdump" -- not timing, not a race.
    //
    // Stepping over it is exactly what the debugger does, so the game reaches
    // the same state it reaches under procdump, which has been observed running
    // 6601 frames and exiting cleanly. Continuing is therefore the MEASURED
    // behaviour rather than a hopeful one -- but it is still a raised breakpoint
    // being ignored, so it is logged loudly with a stack the first few times and
    // crash_dump_continue_breakpoint=0 turns it off.
    //
    // RIP: for a trap, Windows reports ExceptionAddress as the int 3 itself and
    // leaves the context pointing at the following byte. Only advance when the
    // two are equal, which is the case where it has not been advanced already.
    // ---------------------------------------------------------------------
    if (code == static_cast<DWORD>(STATUS_BREAKPOINT) && g_config.continueBreakpoint) {
        static std::atomic<int> s_breakpoints{0};
        const int seen = s_breakpoints.fetch_add(1, std::memory_order_relaxed);
        if (seen < 4) {
            char site[MAX_PATH + 64];
            DescribeAddress(pointers->ExceptionRecord->ExceptionAddress, site, sizeof(site));
            Log("[CRASH] breakpoint #%d at %p -> %s on thread %lu; stepping over it\n",
                seen, pointers->ExceptionRecord->ExceptionAddress, site, GetCurrentThreadId());
            LogCallStack("breakpoint call stack");
        }
        if (pointers->ContextRecord) {
            if (reinterpret_cast<void*>(pointers->ContextRecord->Rip) ==
                pointers->ExceptionRecord->ExceptionAddress) {
                pointers->ContextRecord->Rip += 1;
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    if (!IsFatalCode(code)) return EXCEPTION_CONTINUE_SEARCH;

    // Bounded: a fault inside a retry loop would otherwise log without end.
    if (g_eventsHandled.fetch_add(1, std::memory_order_relaxed) >= 32) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ReportFault(pointers, false);
    return EXCEPTION_CONTINUE_SEARCH;  // never swallow -- observe only
}

LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* pointers) {
    if (pointers && pointers->ExceptionRecord) {
        ReportFault(pointers, true);
    }
    return g_previousFilter ? g_previousFilter(pointers) : EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------------------
// Process-exit capture.
//
// The run that prompted this logged NO exception, wrote no dump, produced no WER
// Application Error and no TDR -- the log simply stopped. A process that ends
// that quietly was not crashed, it was EXITED, and the only remaining question
// is by whom. RtlExitUserProcess is the funnel every orderly exit reaches
// (ExitProcess forwards to it, and so does the CRT's exit()); TerminateProcess
// covers the abrupt kind.
// ---------------------------------------------------------------------------
using RtlExitUserProcessFn = void(WINAPI*)(ULONG);
using TerminateProcessFn   = BOOL(WINAPI*)(HANDLE, UINT);

RtlExitUserProcessFn g_realRtlExitUserProcess = nullptr;
TerminateProcessFn   g_realTerminateProcess = nullptr;
std::atomic<bool>    g_exitReported{false};

void WINAPI HookRtlExitUserProcess(ULONG exitCode) {
    if (!g_exitReported.exchange(true)) {
        Log("[CRASH] process exiting via RtlExitUserProcess(0x%08X) on thread %lu\n",
            static_cast<unsigned>(exitCode), GetCurrentThreadId());
        LogCallStack("exit call stack");
    }
    g_realRtlExitUserProcess(exitCode);
}

BOOL WINAPI HookTerminateProcess(HANDLE process, UINT exitCode) {
    // Only our own process matters; the game spawns helpers and killing one of
    // those is routine.
    const bool self = process == GetCurrentProcess() ||
                      GetProcessId(process) == GetCurrentProcessId();
    if (self && !g_exitReported.exchange(true)) {
        Log("[CRASH] process terminated via TerminateProcess(self, 0x%08X) on thread %lu\n",
            static_cast<unsigned>(exitCode), GetCurrentThreadId());
        LogCallStack("terminate call stack");
    }
    return g_realTerminateProcess(process, exitCode);
}

// The one the other two funnel into, and the one that can be reached WITHOUT them. A process that
// ends without RtlExitUserProcess and without kernelbase!TerminateProcess has called this
// directly -- which is what a run ending in silence, with a first-chance fault logged and no
// unhandled filter, looks like from inside.
using NtTerminateProcessFn = LONG(NTAPI*)(HANDLE, LONG);
NtTerminateProcessFn g_realNtTerminateProcess = nullptr;

LONG NTAPI HookNtTerminateProcess(HANDLE process, LONG exitStatus) {
    // A NULL handle means "every thread in this process except the caller" -- the step an orderly
    // shutdown takes just before the real kill, so it counts as ours.
    const bool self = process == nullptr || process == GetCurrentProcess() ||
                      GetProcessId(process) == GetCurrentProcessId();
    if (self && !g_exitReported.exchange(true)) {
        Log("[CRASH] process ending via NtTerminateProcess(%s, status=0x%08X) on thread %lu\n",
            process == nullptr ? "NULL = other threads" : "self",
            static_cast<unsigned>(exitStatus), GetCurrentThreadId());
        LogCallStack("terminate call stack");
    }
    return g_realNtTerminateProcess(process, exitStatus);
}

void InstallExitHooks() {
    const MH_STATUS init = MH_Initialize();  // no-op if another module got there first
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        Log("[CRASH] MH_Initialize failed (%d); exit hooks unavailable\n",
            static_cast<int>(init));
        return;
    }

    if (HMODULE ntdll = GetModuleHandleA("ntdll.dll")) {
        if (void* target = reinterpret_cast<void*>(
                GetProcAddress(ntdll, "RtlExitUserProcess"))) {
            if (MH_CreateHook(target, reinterpret_cast<void*>(&HookRtlExitUserProcess),
                              reinterpret_cast<void**>(&g_realRtlExitUserProcess)) == MH_OK) {
                MH_EnableHook(target);
            }
        }
        // Ordering is deliberate: RtlExitUserProcess reaches NtTerminateProcess, so an orderly
        // exit trips the outer hook first and the once-only guard keeps the inner one quiet. A
        // direct call arrives here with nothing above it, which is the case worth catching.
        if (void* target = reinterpret_cast<void*>(
                GetProcAddress(ntdll, "NtTerminateProcess"))) {
            if (MH_CreateHook(target, reinterpret_cast<void*>(&HookNtTerminateProcess),
                              reinterpret_cast<void**>(&g_realNtTerminateProcess)) == MH_OK) {
                MH_EnableHook(target);
            }
        }
    }
    if (HMODULE kernel = GetModuleHandleA("kernelbase.dll")) {
        if (void* target = reinterpret_cast<void*>(
                GetProcAddress(kernel, "TerminateProcess"))) {
            if (MH_CreateHook(target, reinterpret_cast<void*>(&HookTerminateProcess),
                              reinterpret_cast<void**>(&g_realTerminateProcess)) == MH_OK) {
                MH_EnableHook(target);
            }
        }
    }
    Log("[CRASH] exit hooks: RtlExitUserProcess=%s TerminateProcess=%s NtTerminateProcess=%s\n",
        g_realRtlExitUserProcess ? "on" : "off",
        g_realTerminateProcess ? "on" : "off",
        g_realNtTerminateProcess ? "on" : "off");
}

} // namespace

void InstallCrashHandler() {
    if (g_installed.exchange(true)) return;

    LoadConfig();
    if (!g_config.enabled) {
        Log("[CRASH] handler disabled (crash_dump=0 in vrport.ini)\n");
        return;
    }

    // First in the vectored chain: this runs before any SEH frame, so it sees
    // the fault even if the engine's own filter would go on to swallow it and
    // exit quietly -- which is what the absent WER Application Error event for
    // Cyberpunk2077.exe suggests is happening.
    AddVectoredExceptionHandler(1, VectoredHandler);
    g_previousFilter = SetUnhandledExceptionFilter(UnhandledFilter);
    InstallExitHooks();

    Log("[CRASH] handler armed. dir=%s type=%s max=%d first_chance_dump=%d "
        "continue_breakpoint=%d\n",
        g_config.directory,
        g_config.fullMemory ? "full" : "referenced",
        g_config.maxDumps,
        g_config.dumpFirstChance ? 1 : 0,
        g_config.continueBreakpoint ? 1 : 0);
}
