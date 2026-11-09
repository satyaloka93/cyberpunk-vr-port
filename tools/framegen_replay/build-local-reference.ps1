$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$c='C:\Program Files\CMake\bin\cmake.exe'
& $c -S "$root\tools\framegen_replay" -B "$root\build-bike-replay" -A x64 '-DOPENXR_INCLUDE=F:/ai/GITHUB/cyberpunk-vr-port/build/combined-port/_deps/openxr-src/include' '-DFFX_SDK=F:/ai/GITHUB/cyberpunk-vr-port/build/fixes-019-msvc/fidelityfx-native/sdk' '-DV557_SOURCE=F:/ai/GITHUB/OFXR-Bridge/build/cyberpunk-v557-backend' '-DV557_BUILD=F:/ai/GITHUB/OFXR-Bridge/build/cyberpunk-v557-backend/build-readiness-tests'
if($LASTEXITCODE){exit $LASTEXITCODE}
& $c --build "$root\build-bike-replay" --config Release --parallel 3
exit $LASTEXITCODE
