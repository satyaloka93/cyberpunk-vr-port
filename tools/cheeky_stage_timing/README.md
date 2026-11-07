# Companion Cheeky stage-timing patch

This is the separately deployed **diagnostic-only** Cheeky change used to attribute
source GPU cost during the port's framegen investigation. It is not compiled
into CyberpunkVRPort. It is preserved here so the final measurement is reproducible;
this does not claim the patch was committed to or published from the Cheeky fork.

`source-provenance.json` identifies the exact local source snapshot and baseline
runtime by hash. The baseline runtime byte-matched the installed runtime. All
source/header hashes and isolated MSBuild build/deployment receipts remain in
the local `build/cheeky-stage-timing-20261107` archive. A byte-matched old binary
plus source timestamps is provenance evidence, not a reproducible-build guarantee.

Apply `git apply -p0 <path>/hooks.patch` from a compatible Cheeky source root and
copy stage_timing_export.hpp to that root's src directory. Check the source hashes
first; do not silently apply to a different fork/build. Build the runtime using
Cheeky's normal project. The tested deployment replaced only the runtime DLL.
No loaders, NR vendor libraries, rendering settings or GPU sampling were changed.

One new line every5s in CheekyFoveatedDLSS-Standalone.log carries nine cumulative
sampled GPU duration/count pairs and PID/QPC. Existing sampling is125ms/category.
Run `python analyze_stages.py <log>` to compute fresh interval means. Zero fresh
samples returns null. Reset/first records establish baselines. Pipeline timers
contain children; do not add pipeline and SR/NR or multiply averages into an exact
stereo frame cost. Evaluations are not eye/frame-paired; GPU completion may land
in a later interval. These are GPU spans under concurrent workload, not FPS.

`python test_analysis.py` checks the captured real formatter fixture, stale samples,
resets and invalid records. On Windows, build StageTimingTests.vcxproj with
`/p:Configuration=Release /p:Platform=x64 /p:CheekySource=<source-root>` to exercise
the formatter against actual diagnostics.cpp. Candidate runtime also passed
Cheeky's existing runtime-host lifecycle smoke test and emitted this record from
its Present path; this test alone does not validate live GPU duration accuracy.

Latest headset test:531 samples/category over75s, SR1.594ms, foveated NR2.541ms,
containingpipeline4.147ms. NR includes guides/encode, neural evaluation and
filter/decode/composite, not just NVIDIA evaluation. See
[the port baseline](../../docs/FRAMEGEN-BASELINE-20261107.md).
