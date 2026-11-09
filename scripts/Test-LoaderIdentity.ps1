#requires -Version 5.1
param([string]$ReferenceGameRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Loader-Identity.ps1')
$root=Join-Path ([IO.Path]::GetTempPath()) ('cpvr-loader-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $root 'bin\x64') -Force | Out-Null
$passed=0
function Assert($condition,$message) { if(-not $condition){throw "FAIL: $message"}; $script:passed++; Write-Host "PASS: $message" }
try {
    $catalog=@(Get-LoaderCatalog)
    Assert ((Get-LoaderIdentity $root 'bin\x64\winmm.dll').State -eq 'MISSING') 'Missing loader is reported'
    Assert (-not (Test-Red4extIdentity $root)) 'Missing pair is not accepted'
    Assert-Red4extRepairSafe $root
    $file=Join-Path $root 'bin\x64\winmm.dll'
    [IO.File]::WriteAllText($file,'Not RED4ext')
    $before=(Get-FileHash $file).Hash
    Assert ((Get-LoaderIdentity $root 'bin\x64\winmm.dll').State -eq 'UNKNOWN') 'Arbitrary file is not trusted by name'
    $blocked=$false; try { Assert-Red4extRepairSafe $root } catch { $blocked=$_.Exception.Message -match 'No game files changed' }
    Assert $blocked 'Unknown winmm blocks repair with actionable explanation'
    Assert ((Get-FileHash $file).Hash -eq $before) 'Blocked preflight preserves unknown bytes'
    $fake=@([pscustomobject]@{path='bin/x64/winmm.dll';identity='Test fixture';sha256=$before;version=$null})
    Assert ((Get-LoaderIdentity $root 'bin\x64\winmm.dll' $fake).State -eq 'VERIFIED') 'Hash recognizes versionless file without inventing a version'
    $fake[0].version='1.31.0'
    Assert ((Get-LoaderIdentity $root 'bin\x64\winmm.dll' $fake).State -eq 'VERSION-MISMATCH') 'Hash does not bypass version check'
    if($ReferenceGameRoot) {
        foreach($rel in @('bin\x64\dxgi.dll','bin\x64\version.dll','bin\x64\winmm.dll','red4ext\RED4ext.dll')) {
            $out=Join-Path $root $rel
            New-Item -ItemType Directory -Path (Split-Path -Parent $out) -Force | Out-Null
            Copy-Item -LiteralPath (Join-Path $ReferenceGameRoot $rel) -Destination $out -Force
            Assert ((Get-LoaderIdentity $root $rel).State -eq 'VERIFIED') "Real file matches hash and version: $rel"
        }
        Assert (Test-Red4extIdentity $root) 'Real RED4ext pair passes'
        Copy-Item -LiteralPath (Join-Path $root 'bin\x64\version.dll') -Destination $file -Force
        Assert ((Get-LoaderIdentity $root 'bin\x64\winmm.dll').State -eq 'WRONG-LOCATION') 'Renamed ASI loader identified in winmm slot'
        Assert (-not (Test-Red4extIdentity $root)) 'ASI loader cannot satisfy RED4ext requirement'
        $blocked=$false; try { Assert-Red4extRepairSafe $root } catch { $blocked=$_.Exception.Message -match 'Ultimate ASI Loader' }
        Assert $blocked 'Collision error names the actual ASI loader'
        Show-LoaderIdentityReport $root
    }
    Write-Host "$passed loader identity assertions passed. Real game unchanged."
} finally { Remove-Item -LiteralPath $root -Recurse -Force }
