#requires -Version 5.1
. (Join-Path $PSScriptRoot 'Inspect-OpenXRLayers.ps1')
# Read-only inspection. Keep session evidence separate from inferred causes.
if (-not ('CpvrLogTail' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
public sealed class CpvrLogTail {
    public string[] Lines;
    public long Offset, Length;
    public static CpvrLogTail Read(string path, int limit) {
        using (var f = new FileStream(path, FileMode.Open, FileAccess.Read,
                    FileShare.ReadWrite | FileShare.Delete)) {
            long length = f.Length, offset = Math.Max(0, length-limit);
            f.Position=offset;
            byte[] bytes=new byte[(int)(length-offset)]; int used=0, n;
            while(used<bytes.Length && (n=f.Read(bytes,used,bytes.Length-used))>0) used+=n;
            var lines=new List<string>(); int start=0; bool partial=offset>0;
            for(int i=0;i<=used;i++) {
                if(i!=used && bytes[i]!=10) continue;
                int end=i; bool bigEndian=false;
                if(!partial && end>start) {
                    int zeros=0; for(int j=start;j<end;j++) if(bytes[j]==0) zeros++;
                    string line;
                    if(zeros>(end-start)/8) {
                        bool be=bytes[start]==0 || (end-start>1 && bytes[start]==254 && bytes[start+1]==255);
                        bigEndian=be;
                        // Big-endian newline contributes a trailing zero before LF.
                        if(be && end>start && bytes[end-1]==0) end--;
                        line=(be?Encoding.BigEndianUnicode:Encoding.Unicode).GetString(bytes,start,(end-start)&~1);
                    } else line=Encoding.UTF8.GetString(bytes,start,end-start);
                    lines.Add(line.Trim('\r','\n','\uFEFF'));
                } else if(!partial) lines.Add("");
                partial=false; start=i+1;
                // UTF-16LE LF is 0A 00. Consume its second byte, but not a BE character.
                if(!bigEndian && i>0 && bytes[i-1]==0 && start<used && bytes[start]==0) {start++;i++;}
            }
            return new CpvrLogTail { Lines=lines.ToArray(), Offset=offset, Length=length };
        }
    }
}
'@
}

function Read-CpvrDiagnosticLog {
    param([string]$Path, [ValidateRange(4096,16777216)][int]$MaxBytes=2097152)
    $r=[ordered]@{Path=$Path; State='MISSING'; ModifiedUtc=$null; Size=0; Offset=0; Lines=@(); ReadError=$null}
    try {
        try {$null=[IO.File]::GetAttributes($Path)}
        catch [IO.FileNotFoundException] {return [pscustomobject]$r}
        catch [IO.DirectoryNotFoundException] {return [pscustomobject]$r}
        $r.ModifiedUtc=[IO.File]::GetLastWriteTimeUtc($Path)
        $tail=[CpvrLogTail]::Read($Path,$MaxBytes)
        $r.Size=$tail.Length; $r.Offset=$tail.Offset; $r.Lines=@($tail.Lines); $r.State='READ'
    } catch {$r.State='UNREADABLE';$r.ReadError=$_.Exception.Message}
    return [pscustomobject]$r
}

function Get-CpvrLogFindings {
    param($Log, [datetime]$NowUtc=[datetime]::UtcNow, [Nullable[datetime]]$AnchorUtc=$null)
    $lines=@($Log.Lines); $start=0; $sessionPid=$null; $sessionTime=$null; $marker=$null
    # Last explicit initialization boundary only; do not mix earlier failed launches.
    for($i=0;$i -lt $lines.Count;$i++) {
        if($lines[$i] -match 'PROCESS_ATTACH bootstrap-log|Initializing (standalone|OptiScaler) host|Cheeky .+ (Standalone|OptiScaler) initializing|=== CyberpunkVRPort.*plugin (loaded\b|initializ)|Starting RED4ext|Starting redscript') {
            $start=$i;$marker=$lines[$i]
        }
    }
    if($marker) {
        if($marker -match '\bpid=(\d+)'){$sessionPid=$Matches[1]}
        if($marker -match '\[(\d{4}-\d\d-\d\dT[^\s\]]+Z)') {
            $parsed=[datetimeoffset]::MinValue
            if([datetimeoffset]::TryParse($Matches[1],[ref]$parsed)){$sessionTime=$parsed.UtcDateTime}
        }
    }
    $freshness='UNCORRELATED'; $reason='No dated session/PID correlation; file modification time is not proof of the same launch.'
    if($sessionTime){$freshness='DATED SESSION';$reason='Session boundary timestamp available.'}
    if(($Log.ModifiedUtc -and ($NowUtc-$Log.ModifiedUtc).TotalHours -gt 24) -or
       ($sessionTime -and ($NowUtc-$sessionTime).TotalHours -gt 24) -or
       ($AnchorUtc -and $Log.ModifiedUtc -and $Log.ModifiedUtc -lt $AnchorUtc.AddMinutes(-10)) -or
       ($AnchorUtc -and $sessionTime -and $sessionTime -lt $AnchorUtc.AddMinutes(-10))) {
        $freshness='STALE';$reason='Older than the latest dated loader session or more than 24 hours old; do not attribute to the current failure.'
    }
    $findings=New-Object 'System.Collections.Generic.List[object]';$seen=@{};$recoveries=New-Object 'System.Collections.Generic.List[object]'
    for($i=$start;$i -lt $lines.Count;$i++) {
        $line=$lines[$i]
        if($sessionPid -and $line -match '\bpid=(\d+)' -and $Matches[1] -ne $sessionPid){continue}
        $success=$line -match 'host started successfully|Host ready;|Menu ready|DLSS-NR runtime search path=.*error=0\b|fallback.*(succeed|available|loaded)|using .*fallback|Shutdown complete|plugin unloaded'
        if($success){$recoveries.Add([pscustomobject]@{Line=$i+1;Text=$line.Substring(0,[Math]::Min(900,$line.Length))})}
        $kind=$null;$advice=$null
        if($line -match '(?i)\bFATAL\b|fail[- ]fast|unhandled exception|device[_ ](removed|hung)|DXGI_ERROR_DEVICE') {
            $kind='FATAL/GPU';$advice='Keep this session log and crash report. This line alone does not identify the responsible mod.'
        } elseif($line -match 'missing or unloadable|export (missing|unresolved)|Runtime missing or API incompatible|LoadLibrary.*fail|failed to load.*(DLL|library)|host start failed|host initialization failed|startup timed out|graphics hook discovery failed|runtime refused startup|could not (pin bootstrap|start the host)|cannot locate bootstrap|startup exception contained') {
            $kind='LOADER/HOST';$advice='Check the matching loader, Host and Runtime files from one Cheeky package. Review the Win32 code and later startup evidence; do not replace unrelated proxy DLLs.'
        } elseif($line -match 'compilation (has )?failed|failed to compile|invalid native definitions|missing dependency|dependency.*not found') {
            $kind='DEPENDENCY/SCRIPT';$advice='Review the named mod/dependency and its version; the message does not prove Cheeky caused it.'
        } elseif($line -match '(xrCreateInstance|xrCreateSession|OpenXR.*init).*fail|XR_ERROR_') {
            $kind='OPENXR';$advice='Review the selected runtime and layer logs for this same session; no registry changes are made here.'
        } elseif($line -match '\b(error|failed|failure)\b|\berror=[1-9]\d*') {
            # Every early Cheeky line includes error=0; successful fallback text is evidence, not a fatal error.
            $clean=$line -replace '\berror=0\b','' -replace '\b(errors?|failed|failures?)\s*[:=]?\s*0\b','' -replace '\b0\s+(errors?|failures?)\b',''
            if(-not $success -and $clean -match '\b(error|failed|failure)\b|\berror=[1-9]\d*') {
                $kind='CANDIDATE';$advice='Keyword match only. Read the context and later success messages before diagnosing a conflict.'
            }
        }
        if(-not $kind){continue}
        $key=($line -replace '^\[[^\]]*\]\s*','' -replace '^\d\d:\d\d:\d\d\.\d+\s*','' -replace '\[T\d+\]\s*','').Trim()
        if($seen.ContainsKey($key)){$seen[$key].Count++;$seen[$key].LastLine=$i+1;continue}
        if($findings.Count -ge 24){
            # Generic startup chatter must not hide a later fatal/loader failure.
            $replace=$null
            if($kind -ne 'CANDIDATE'){$replace=$findings | Where-Object Kind -eq 'CANDIDATE' | Select-Object -First 1}
            if(-not $replace -and $kind -eq 'FATAL/GPU'){$replace=$findings | Where-Object Kind -ne 'FATAL/GPU' | Select-Object -First 1}
            if(-not $replace){continue}
            $null=$findings.Remove($replace);$seen.Remove($replace.Key)
        }
        $context=@();for($j=[Math]::Max($start,$i-1);$j -le [Math]::Min($lines.Count-1,$i+1);$j++){
            $context+=('{0}: {1}' -f ($j+1),$lines[$j].Substring(0,[Math]::Min(900,$lines[$j].Length)))
        }
        $item=[pscustomobject]@{Key=$key;Kind=$kind;Line=$i+1;LastLine=$i+1;Count=1;Text=$line.Substring(0,[Math]::Min(900,$line.Length));Context=$context;Advice=$advice}
        $findings.Add($item);$seen[$key]=$item
    }
    return [pscustomobject]@{Log=$Log;Freshness=$freshness;Reason=$reason;SessionPid=$sessionPid;SessionUtc=$sessionTime;SessionMarker=$marker;StartLine=$start+1;Findings=@($findings.ToArray());SuccessEvidence=@($recoveries.ToArray() | Select-Object -Last 4)}
}

function Get-CpvrInstallationDiagnostics {
    param([Parameter(Mandatory=$true)][string]$Root)
    $paths=@('bin\x64\CheekyFoveatedDLSS-Loader.log',
        'bin\x64\CheekyFoveatedDLSS\CheekyFoveatedDLSS-Host.log',
        'bin\x64\CheekyFoveatedDLSS\CheekyFoveatedDLSS-Standalone.log',
        'red4ext\plugins\cyberpunkvrport\log\cyberpunkvrport.log',
        'red4ext\plugins\CyberpunkVR_Stereo\cyberpunkvrport.log','bin\x64\cyberpunkvrport.log',
        'red4ext\logs\red4ext.log','r6\logs\redscript_rCURRENT.log')
    # RED4ext also uses versioned/date-named logs. Bound discovery and exclude unrelated games.
    $red=Join-Path $Root 'red4ext\logs'
    if(Test-Path -LiteralPath $red){$paths+=@(Get-ChildItem -LiteralPath $red -Filter 'red4ext*.log' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1 | ForEach-Object {$_.FullName.Substring($Root.TrimEnd('\').Length+1)})}
    $logs=@($paths | Select-Object -Unique | ForEach-Object {Read-CpvrDiagnosticLog (Join-Path $Root $_)})
    $loader=Get-CpvrLogFindings $logs[0];$anchor=$loader.SessionUtc
    $results=@($logs | ForEach-Object {Get-CpvrLogFindings $_ -AnchorUtc $anchor})
    $identities=@();$catalog=@(Get-LoaderCatalog)
    foreach($rel in @('bin\x64\dxgi.dll','bin\x64\dxgi2.dll','bin\x64\version.dll','bin\x64\winmm.dll','red4ext\RED4ext.dll')) {
        try {$identities+=Get-LoaderIdentity $Root $rel $catalog} catch {$identities+=[pscustomobject]@{Path=$rel;State='UNREADABLE';Identity=$_.Exception.Message;Version=$null;Sha256=$null;ExpectedPath=$null}}
    }
    $hints=New-Object 'System.Collections.Generic.List[string]'
    $cheeky=@($identities | Where-Object {$_.Identity -match 'Cheeky' -and $_.Path -match '(dxgi|version)\.dll$'})
    if($cheeky.Count -gt 1){$hints.Add('POSSIBLE DUPLICATE CHEEKY PROXIES: recognized Cheeky files occupy both DXGI and VERSION slots. These are alternative entry points; review your package instructions. Files are preserved.')}
    if(@($identities | Where-Object {$_.State -eq 'WRONG-LOCATION'}).Count){$hints.Add('MISPLACED KNOWN LOADER: review expected location above; do not rename/delete another mod automatically.')}
    if(@($identities | Where-Object {$_.State -eq 'UNKNOWN'}).Count){$hints.Add('UNKNOWN is not proof of damage or conflict. A newer/custom DLL needs provenance before identifying it.')}
    if($logs[0].State -eq 'MISSING'){$hints.Add('Missing Cheeky loader log: possible older loader, proxy never loaded, failure before entry, or unwritable directory. This is not proof of a mod conflict.')}
    $hints.Add('Cheeky DXGI and VERSION are alternatives. Do not overwrite an ASI Loader version.dll or RED4ext winmm.dll. The switcher does not install or repair Cheeky.')
    return [pscustomobject]@{GeneratedUtc=[datetime]::UtcNow;GameRoot=$Root;Identities=$identities;OpenXR=(Get-CpvrOpenXrDiagnostics -Root $Root);Logs=$results;Hints=@($hints.ToArray())}
}

function Format-CpvrInstallationDiagnostics {
    param($Report)
    "Cyberpunk / Cheeky installation and recent errors (READ-ONLY)"
    "Game: $($Report.GameRoot)"
    "Report UTC: $($Report.GeneratedUtc.ToString('o'))"
    'A file identity is not runtime compatibility. Matches are evidence, not automatic diagnoses.'
    foreach($item in $Report.Identities){
        $version=if($item.Version){$item.Version}else{'no embedded version'}
        "[$($item.State)] $($item.Path): $($item.Identity); $version"
        if($item.Sha256){"  SHA-256: $($item.Sha256)"}
        if($item.ExpectedPath){"  Catalog location: $($item.ExpectedPath)"}
    }
    foreach($hint in $Report.Hints){"NOTE: $hint"}
    Format-CpvrOpenXrDiagnostics $Report.OpenXR
    foreach($r in $Report.Logs){
        $l=$r.Log;'';"[$($l.State)] $($l.Path)"
        if($l.State -eq 'MISSING'){'  Log missing.';continue}
        if($l.State -eq 'UNREADABLE'){"  Log unreadable: $($l.ReadError)";continue}
        "  Modified UTC: $($l.ModifiedUtc.ToString('o')); size: $($l.Size) bytes; [$($r.Freshness)] $($r.Reason)"
        if($r.SessionMarker){"  Latest retained session: $($r.SessionMarker)"}
        if($l.Offset){"  Bounded tail begins at byte $($l.Offset). Line numbers are TAIL-relative; earlier history was not scanned."}
        else {'  Line numbers refer to this file snapshot.'}
        if(-not $r.Findings.Count){'  No matching errors found in the inspected session/tail. This does not prove compatibility.'}
        foreach($f in $r.Findings){
            "  [$($f.Kind)] line $($f.Line), occurrences $($f.Count), last line $($f.LastLine)"
            foreach($c in $f.Context){"    $c"}
            "    Next: $($f.Advice)"
        }
        if($r.Findings.Count -ge 24){'  Display capped at 24 distinct findings; inspect the source log for additional messages.'}
        foreach($v in $r.SuccessEvidence){"  SUCCESS/RECOVERY EVIDENCE line $($v.Line): $($v.Text)"}
        if($r.Findings.Count -and $r.SuccessEvidence.Count){'  Success messages may show recovery of one component only; they do not clear unrelated failures.'}
    }
}

function Get-CpvrFindingDisposition($Session,$Finding) {
    # Suppression is narrow and presentation-only. Full findings remain in the export.
    if($Finding.Kind -ne 'CANDIDATE'){return 'Review'}
    if($Finding.Text -match "HookIAT: Failed to find function '(MoveWindow|GetCursorInfo|GetSystemMetrics|GetMessagePos)' in DLL 'user32\.dll' for module [0-9A-Fa-f]+\s*$") {return 'Optional hook'}
    if($Finding.Text -match 'DLSS-NR runtime search path=.+error=[1-9]\d*\s*$') {
        $lines=@($Session.Log.Lines)
        $failed=$lines[$Finding.LastLine-1]
        $thread=if($failed -match '\[T(\d+)\]'){$Matches[1]}else{$null}
        $findingPid=if($failed -match '\bpid=(\d+)'){$Matches[1]}else{$null}
        for($i=$Finding.LastLine;$i -lt [Math]::Min($lines.Count,$Finding.LastLine+6);$i++) {
            $line=$lines[$i]
            if($thread -and $line -notmatch ('\[T'+[regex]::Escape($thread)+'\]')){continue}
            if($findingPid -and $line -notmatch ('\bpid='+[regex]::Escape($findingPid)+'\b')){continue}
            if($line -match 'DLSS-NR runtime selection=|Initializing|PROCESS_ATTACH|\bFATAL\b'){break}
            if($line -match 'DLSS-NR runtime search path=.+error=0\s*$'){return 'Recovered search'}
        }
    }
    if($Finding.Count -eq 1 -and $Finding.Text -match 'OpenXRManager: Mono capture failed\. serial=1 views=\(0,0\)\s*$') {
        # Surface creation is startup progress, not proof later frames displayed successfully.
        $lines=@($Session.Log.Lines)
        if($Finding.LastLine -lt $lines.Count -and $lines[$Finding.LastLine] -match 'OpenXRManager: eye image [1-9]\d*x[1-9]\d* '){return 'Startup observation'}
    }
    return 'Review'
}
function Format-CpvrDiagnosticSummary($Report) {
    'Cyberpunk / Cheeky quick check (READ-ONLY)'
    "Game: $($Report.GameRoot)"
    $sessions=@($Report.Logs | Where-Object {$_.Log.State -eq 'READ' -and $_.Freshness -ne 'STALE'})
    $dated=@($sessions | Where-Object {$_.SessionUtc} | Sort-Object SessionUtc -Descending | Select-Object -First 1)
    if($dated.Count){"Latest dated log session: $($dated[0].SessionUtc.ToString('u')) (not a live-process check)"}
    else {'No recent dated session found; log findings may not describe your current launch.'}
    $red=@($Report.Identities | Where-Object {$_.Path -in @('bin\x64\winmm.dll','red4ext\RED4ext.dll')})
    if($red.Count -eq 2 -and @($red | Where-Object State -ne 'VERIFIED').Count -eq 0){'OK: RED4ext loader/framework identities verified.'}
    foreach($item in $Report.Identities) {
        if($item.State -eq 'MISSING' -and $item.Path -notin @('bin\x64\winmm.dll','red4ext\RED4ext.dll')){continue}
        if($item.State -eq 'VERIFIED' -and $item.Path -in @('bin\x64\winmm.dll','red4ext\RED4ext.dll')){continue}
        "[$($item.State)] $($item.Path): $($item.Identity)"
    }
    $xr=$Report.OpenXR
    $cheeky=@($xr.Layers | Where-Object {$_.Name -match '(?i)cheeky' -or $_.Manifest -match '(?i)cheeky'})
    $enabled=@($cheeky | Where-Object Registration -eq 'ENABLED-REGISTRATION')
    "Cheeky OpenXR: $($enabled.Count) enabled implicit registration(s)."
    foreach($l in $cheeky){
        if($l.Registration -eq 'ENABLED-REGISTRATION'){
            $dll=if($l.Binary){$l.Binary.Path}else{$l.Manifest}
            "  [$($l.State)] $dll"
        }
    }
    $other=@($xr.Layers | Where-Object {$_.Registration -eq 'ENABLED-REGISTRATION' -and $_ -notin $cheeky} | ForEach-Object {$_.Name})
    if($other.Count){'Other enabled layers: '+($other -join ', ')}
    foreach($l in $xr.Layers | Where-Object {$_.Registration -eq 'INVALID' -or ($_.Registration -in @('ENABLED-REGISTRATION','EXPLICIT-AVAILABLE') -and $_.State -ne 'READ')}) {"REVIEW layer: $($l.Registration) / $($l.State): $($l.Manifest) $($l.Error)"}
    foreach($hint in $xr.Hints | Where-Object {$_ -match '^(POSSIBLE DUPLICATE|REPEATED REGISTRATION|NO READABLE|DIFFERENT CHEEKY|CHEEKY \+ OFXR)'}){$hint}
    foreach($scanError in $xr.Errors){"SCAN INCOMPLETE: $scanError"}
    if($xr.Environment.Count){
        'Environment overrides (diagnostic process/User/Machine; game inheritance may differ):'
        foreach($e in $xr.Environment | Select-Object -First 6){"  [$($e.Scope)] $($e.Name)=$($e.Value)"}
        if($xr.Environment.Count -gt 6){'  More overrides are included in the detailed report.'}
    }
    foreach($hint in $Report.Hints | Where-Object {$_ -match '^(POSSIBLE DUPLICATE CHEEKY|MISPLACED KNOWN|Missing Cheeky loader log)'}){$hint}
    $review=New-Object 'System.Collections.Generic.List[object]'
    $optional=0;$recovered=0;$startup=0
    foreach($r in $sessions){
        foreach($f in $r.Findings){
            switch(Get-CpvrFindingDisposition $r $f){
                'Optional hook' {$optional+=$f.Count}
                'Recovered search' {$recovered+=$f.Count}
                'Startup observation' {$startup+=$f.Count}
                default {$review.Add([pscustomobject]@{Session=$r;Finding=$f})}
            }
        }
    }
    $ready=@($sessions | ForEach-Object {$_.SuccessEvidence} | Where-Object {$_.Text -match 'host started successfully|Host ready;|Menu ready'})
    if($ready.Count){'Cheeky startup: Host/Menu readiness appears in the inspected logs.'}
    if($recovered){"Recovered: $recovered NR DLL search failure(s), followed by a nearby successful search."}
    if($optional){"Hidden: $optional known optional Win32 import-hook message(s)."}
    if($startup){"Startup: $startup initial empty mono capture(s), followed by eye-surface creation; not a sustained-failure diagnosis."}
    $unread=@($Report.Logs | Where-Object {$_.Log.State -eq 'UNREADABLE'})
    foreach($r in $unread){"UNREADABLE log: $($r.Log.Path): $($r.Log.ReadError)"}
    if($review.Count){
        "Log findings to review: $($review.Count) distinct."
        foreach($entry in $review | Sort-Object @{Expression={switch($_.Finding.Kind){'FATAL/GPU'{0};'LOADER/HOST'{1};'OPENXR'{2};'DEPENDENCY/SCRIPT'{3};default{4}}}} | Select-Object -First 8){
            $f=$entry.Finding;$r=$entry.Session
            $where=if($r.Log.Offset){'tail line'}else{'line'}
            "  [$($f.Kind); $($r.Freshness)] $([IO.Path]::GetFileName($r.Log.Path)) $where $($f.Line), x$($f.Count): $($f.Text)"
        }
        if($review.Count -gt 8){'  Additional findings are retained in the detailed report.'}
    } else {'No unresolved error signatures in the inspected recent log tails. This does not verify live compatibility.'}
    $stale=@($Report.Logs | Where-Object {$_.Log.State -eq 'READ' -and $_.Freshness -eq 'STALE'})
    $missing=@($Report.Logs | Where-Object {$_.Log.State -eq 'MISSING'})
    if($stale.Count -or $missing.Count){"Omitted from summary: $($stale.Count) stale log(s), $($missing.Count) absent log location(s), including alternate/legacy paths."}
    'Registration is not proof a layer loaded. Full paths, hashes and error context remain available with -DetailedDiagnostics or -ReportPath.'
}

function Show-CpvrInstallationDiagnostics {
    param([string]$Root,[string]$ReportPath,[switch]$DetailedDiagnostics)
    # Export only on explicit request, never into the game tree and never over an existing file.
    if($ReportPath){
        $dest=[IO.Path]::GetFullPath($ReportPath);$prefix=[IO.Path]::GetFullPath($Root).TrimEnd('\')+'\'
        if($dest.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Diagnostic report must be outside the game folder.'}
        if(Test-Path -LiteralPath $dest){throw 'Report already exists; choose a new filename.'}
        # Reparse-point parents can resolve into the game directory despite the lexical path.
        $parent=Get-Item -LiteralPath (Split-Path -Parent $dest)
        for($p=$parent;$null -ne $p;$p=$p.Parent){if($p.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Use a report directory without junctions or symbolic links.'}}
    }
    $report=Get-CpvrInstallationDiagnostics $Root
    $summary=@(Format-CpvrDiagnosticSummary $report)
    $text=@($summary)
    if($DetailedDiagnostics -or $ReportPath){$text+=@("", "DETAILED EVIDENCE", "")+@(Format-CpvrInstallationDiagnostics $report)}
    if($DetailedDiagnostics){$text | ForEach-Object {Write-Host $_}}else{$summary | ForEach-Object {Write-Host $_}}
    if($ReportPath){
        $stream=[IO.File]::Open($dest,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
        try {$bytes=[Text.Encoding]::UTF8.GetBytes(($text -join [Environment]::NewLine));$stream.Write($bytes,0,$bytes.Length)} finally {$stream.Dispose()}
        Write-Host "Local report saved: $dest. Contains local paths/usernames; review before sharing. Nothing uploaded."
    }
}
