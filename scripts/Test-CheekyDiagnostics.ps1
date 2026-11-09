#requires -Version 5.1
param([string]$PackagedRoot)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Loader-Identity.ps1')
. (Join-Path $PSScriptRoot 'Inspect-CheekyLogs.ps1')
$base=Join-Path $env:TEMP ('cpvr-diagnostics-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $base 'game';$loader=Join-Path $root 'bin\x64\CheekyFoveatedDLSS-Loader.log'
$passed=0
function Assert($v,$label){if(-not $v){throw "FAIL: $label"};$script:passed++;Write-Host "PASS: $label"}
function Put($path,$text){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null;[IO.File]::WriteAllText($path,$text)}
function Analyze {Get-CpvrLogFindings (Read-CpvrDiagnosticLog $loader)}
function Snap { @(Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object FullName | ForEach-Object {"$($_.FullName) $((Get-FileHash -LiteralPath $_.FullName).Hash)"}) -join "`n" }
try {
    Put (Join-Path $root 'bin\x64\Cyberpunk2077.exe') 'fixture'
    Put (Join-Path $root 'bin\x64\dxgi.dll') 'unknown-loader-preserve'
    Put (Join-Path $root 'personal.ini') 'setting=keep'
    Assert ((Read-CpvrDiagnosticLog $loader).State -eq 'MISSING') 'Missing log distinct from no errors'
    $stamp=[datetime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.fffZ')
    Put $loader "[2020-01-01T00:00:00.000Z pid=1 tid=1 error=126] PROCESS_ATTACH bootstrap-log-v1`nFATAL: old launch`n[$stamp pid=2 tid=2 error=0] PROCESS_ATTACH bootstrap-log-v1`n[$stamp pid=2 tid=2 error=0] host started successfully`n"
    $r=Analyze
    Assert ($r.Findings.Count -eq 0) 'Old fatal launch excluded; error=0 is not an error'
    Assert ($r.SessionPid -eq '2' -and $r.SessionUtc -and $r.Freshness -eq 'DATED SESSION') 'Latest loader PID and UTC recognized'
    Assert ($r.SuccessEvidence.Count -eq 1) 'Successful startup evidence retained'
    [IO.File]::AppendAllText($loader,"[$stamp pid=2 tid=2 error=126] Cheeky: missing or unloadable CheekyFoveatedDLSS/CheekyFoveatedDLSSHost.dll.`n[$stamp pid=2 tid=2 error=126] Cheeky: missing or unloadable CheekyFoveatedDLSS/CheekyFoveatedDLSSHost.dll.`n[$stamp pid=2 tid=2 error=0] host started successfully`n")
    $r=Analyze
    Assert ($r.Findings.Count -eq 1 -and $r.Findings[0].Count -eq 2) 'Repeated errors grouped with count'
    Assert ($r.Findings[0].Kind -eq 'LOADER/HOST' -and $r.Findings[0].Context.Count -ge 2) 'Actionable host error and line context'
    Assert ($r.SuccessEvidence[-1].Line -gt $r.Findings[0].LastLine) 'Later success evidence does not discard earlier error'
    $old=Get-CpvrLogFindings (Read-CpvrDiagnosticLog $loader) -NowUtc ([datetime]::UtcNow.AddDays(2))
    Assert ($old.Freshness -eq 'STALE') 'Stale session identified'
    $h=Join-Path $root 'bin\x64\CheekyFoveatedDLSS\CheekyFoveatedDLSS-Host.log'
    Put $h "10:00:00 Initializing standalone host`n10:00:01 FATAL old host`n11:00:00 Initializing standalone host`n11:00:01 Host ready; waiting for graphics`n"
    $r=Get-CpvrLogFindings (Read-CpvrDiagnosticLog $h)
    Assert ($r.Findings.Count -eq 0 -and $r.Freshness -eq 'UNCORRELATED') 'Undated appended host sessions isolated without invented correlation'
    [IO.File]::SetLastWriteTimeUtc($h,[datetime]::UtcNow.AddHours(-2))
    $r=Get-CpvrLogFindings (Read-CpvrDiagnosticLog $h) -AnchorUtc ([datetime]::UtcNow)
    Assert ($r.Freshness -eq 'STALE') 'Old component log marked stale against loader start'
    foreach($encoding in @([Text.Encoding]::UTF8,[Text.Encoding]::Unicode,[Text.Encoding]::BigEndianUnicode)) {
        [IO.File]::WriteAllText($loader,"FATAL: test failure`r`nhost started successfully`r`n",$encoding)
        $r=Analyze;Assert ($r.Findings.Count -eq 1 -and $r.SuccessEvidence.Count -eq 1) "Decode $($encoding.WebName)"
    }
    [IO.File]::WriteAllText($loader,"old wide line`r`n",[Text.Encoding]::Unicode)
    [IO.File]::AppendAllText($loader,"FATAL: mixed append`r`nhost started successfully`r`n",[Text.Encoding]::UTF8)
    Assert ((Analyze).Findings.Count -eq 1) 'Mixed UTF16/UTF8 append tolerated'
    [IO.File]::WriteAllBytes($loader,[byte[]](0xE9,32,70,65,84,65,76,58,32,98,97,100,10))
    Assert ((Analyze).Findings.Count -eq 1) 'Legacy non-UTF8 byte does not hide ASCII error signature'
    $stream=[IO.File]::Open($loader,[IO.FileMode]::Create,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
    try {$stream.SetLength(600MB);$stream.Position=$stream.Length-80;$b=[Text.Encoding]::UTF8.GetBytes("`nFATAL: tail of huge log`n");$stream.Write($b,0,$b.Length)}finally{$stream.Dispose()}
    $r=Read-CpvrDiagnosticLog $loader
    Assert ($r.Offset -gt 590MB -and ($r.Size-$r.Offset) -le 2MB) '600MB log read bounded to final2MB'
    Assert ((Get-CpvrLogFindings $r).Findings.Count -eq 1) 'Large-file tail error retained'
    Put $loader "FATAL: lock fixture`n"
    $stream=[IO.File]::Open($loader,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try {Assert ((Read-CpvrDiagnosticLog $loader).State -eq 'UNREADABLE') 'Sharing violation reported without crash'}finally{$stream.Dispose()}
    $stream=[IO.File]::Open($loader,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
    try {Assert ((Read-CpvrDiagnosticLog $loader).State -eq 'READ') 'Active writable log can be inspected'}finally{$stream.Dispose()}
    Put $loader ((1..40 | ForEach-Object {"FATAL: distinct $_"}) -join "`n")
    Assert ((Analyze).Findings.Count -eq 24) 'Distinct finding output capped'
    Put $loader (((1..40 | ForEach-Object {"error: generic $_"}) -join "`n")+"`nFATAL: later crash`n")
    Assert (@((Analyze).Findings | Where-Object Kind -eq 'FATAL/GPU').Count -eq 1) 'Later fatal retained when generic matches fill output cap'
    Put $loader "errors=0; failed 0; 0 errors`nusing System32 fallback`n"
    Assert ((Analyze).Findings.Count -eq 0) 'Zero-error summaries and fallback are not failures'
    Put $loader "FATAL: required system DXGI export unresolved; raising fail-fast`nOpenXR initialization failed XR_ERROR_RUNTIME_FAILURE`nREDScript compilation has failed`n"
    $r=Analyze
    Assert ($r.Findings.Count -eq 3) 'Fatal/OpenXR/script signatures recognized'
    $portLog=Join-Path $root 'red4ext\plugins\cyberpunkvrport\log\cyberpunkvrport.log'
    Put $portLog "=== CyberpunkVRPort red4ext plugin loaded ===`nFATAL: current session`n=== CyberpunkVRPort red4ext plugin unloaded ===`n"
    $r=Get-CpvrLogFindings (Read-CpvrDiagnosticLog $portLog)
    Assert ($r.Findings.Count -eq 1) 'Plugin unloaded does not erase current-session failures'
    Put $h "Initializing standalone host`nDLSS-NR runtime search path=private.dll error=126`nDLSS-NR runtime search path=game.dll error=0`n"
    $r=Get-CpvrLogFindings (Read-CpvrDiagnosticLog $h)
    Assert ($r.Findings.Count -eq 1 -and $r.SuccessEvidence.Count -eq 1) 'DLL search fallback success shown alongside failed probe'
    Put (Join-Path $root 'red4ext\logs\red4ext-2026-11-09.log') 'framework log'
    Put (Join-Path $root 'red4ext\logs\unrelated-newer.log') 'different component'
    $before=Snap;$report=Get-CpvrInstallationDiagnostics $root
    Assert (@($report.Logs | Where-Object {$_.Log.Path -like '*red4ext-2026-11-09.log'}).Count -eq 1) 'Date-named RED4ext log is found independently of unrelated logs'

    Assert (@($report.Identities | Where-Object State -eq 'UNKNOWN').Count -eq 1) 'Unknown DLL reported without attribution by filename'
    $export=Join-Path $base 'report.txt';Show-CpvrInstallationDiagnostics $root $export
    Assert ((Snap) -ceq $before) 'Report generation preserves every fixture game/config byte'
    Assert (Test-Path $export) 'Explicit local report exported'
    $blocked=$false;try {Show-CpvrInstallationDiagnostics $root $export}catch{$blocked=$true}
    Assert $blocked 'Existing reports never overwritten'
    $blocked=$false;try {Show-CpvrInstallationDiagnostics $root (Join-Path $root 'bad-report.txt')}catch{$blocked=$true}
    Assert $blocked 'Export into game tree rejected'
    if($PackagedRoot) {
        $cli=Join-Path $PackagedRoot 'scripts\Switch-CyberpunkVRPort.ps1'
        $target=Join-Path $base 'packaged-report.txt'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cli -Action Diagnose -GameRoot $root -ReportPath $target
        Assert ($LASTEXITCODE -eq 0 -and (Test-Path $target)) 'Packaged main CLI Diagnose dispatch works'
        Assert ((Snap) -ceq $before) 'Packaged option preserves game/config bytes'
        Assert ((Get-Content -LiteralPath $cli -Raw) -match '11 Check installation and recent errors') 'Packaged interactive menu exposes diagnosis'
    }
    Write-Host "$passed Cheeky diagnostics assertions passed."
} finally {Remove-Item -LiteralPath $base -Recurse -Force}
