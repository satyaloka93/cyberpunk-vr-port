#pragma once

// In-process fault capture.
//
// Every crash on this project so far has been read off the last line of
// cyberpunkvrport.log, which tells us where we STOPPED and never what threw. An
// external debugger would tell us, except that attaching one is itself a fix:
// procdump serialises the startup window and the fault goes away. So the capture
// has to live inside the process and cost nothing until it fires.
//
// Install as early as possible -- before the game installs its own top-level
// filter, because a vectored handler registered first runs ahead of every SEH
// frame and sees the exception whether or not something downstream swallows it.
void InstallCrashHandler();
