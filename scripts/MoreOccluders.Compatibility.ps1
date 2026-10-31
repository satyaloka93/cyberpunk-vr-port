# Requires CyberpunkVR-Switcher.Core.ps1. No third-party script is bundled.
function Get-OccludersTextHash([string]$Text) {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Convert-MoreOccluders250([string]$Source, [string]$Destination) {
    $text=[IO.File]::ReadAllText($Source).Replace("`r`n","`n").TrimEnd([char[]]"`r`n")
    $hash=Get-OccludersTextHash $text
    # Normalized original -> patched hashes per Nexus release (mod 33136). Scripted
    # v0.3 (file 157829, uploaded 2026-08-26 16:53 UTC) is the current download; the
    # archived v0.2 script is kept for installs that already have it.
    $known=[ordered]@{
        '66acfa364760da34d2f8c1d79637f336e095eb8144a75698d42706120f2406fd'='23217256f20604c31b4e69bc09696c4bc3e91541a3c1f596df6d3447100aa53f' # Scripted v0.3
        'eb7033f7e619da35a301fb353d895733fc9890f4a836131d72ce072092cd6f6a'='d2a9e04f1e5252cd5e041a5818a877e45e7a2b8a139c0d39c091fe1d744a45de' # v0.2
    }
    $original=$known.Contains($hash)
    if ($original) {
        $start=$text.IndexOf("`t`t`telse`n")
        $end=$text.IndexOf("`t`t`tindex += 1;",$start)
        if($start -lt 0 -or $end -le $start){throw 'Recognized script has unexpected patch boundaries.'}
        $replacement="`t`t`t// 2.50 instanced meshes are cooked nodes; their mesh/occluder`n"+
            "`t`t`t// properties are no longer exposed. Preserve native cooked data.`n`n"
        $text=$text.Substring(0,$start)+$replacement+$text.Substring($end)
        if((Get-OccludersTextHash $text) -ne $known[$hash]){throw 'Patched script did not match the tested game-2.5 result.'}
    } elseif (@($known.Values) -notcontains $hash) {
        throw "Unrecognized MoreOccluders.reds ($hash). Nothing patched. Supported: More Occluders (Scripted) v0.3 from Nexus (or the older v0.2 script). Other or edited scripts need review; the ArchiveXL variant has no script and needs no patch."
    }
    New-Item -ItemType Directory (Split-Path $Destination) -Force|Out-Null
    [IO.File]::WriteAllText($Destination,$text,(New-Object Text.UTF8Encoding($false)))
    return $original
}
function Stage-MoreOccluders250([string]$Root,[string]$Stage,[string]$Archive) {
    $rel='r6\scripts\MoreOccluders.reds'
    $source=Get-SafePath $Root $rel
    if($Archive){
        $check=Join-Path $CacheRoot ('optional-occluders-'+[guid]::NewGuid().ToString('N'))
        Expand-SafeModZip $Archive $check
        $files=@(Get-ChildItem $check -File -Recurse)
        $source=Get-SafePath $check $rel
        if($files.Count -ne 1 -or $files[0].FullName -ine $source){throw 'Select the More Occluders Scripted v0.3 ZIP containing only r6/scripts/MoreOccluders.reds.'}
    }
    if(-not(Test-Path -LiteralPath $source -PathType Leaf)){throw 'No MoreOccluders.reds installed. Supply the optional Scripted v0.3 ZIP using -MoreOccludersArchive.'}
    $changed=Convert-MoreOccluders250 $source (Get-SafePath $Stage $rel)
    if($changed){Write-Host 'STAGE More Occluders game-2.5 patch: ordinary meshes retained; incompatible instanced-mesh edits removed.'}
    else {Write-Host 'More Occluders already matches our tested game-2.5 patch.'}
}
function Install-OptionalOccluders250([string]$Root) {
    if($interactiveMenu -and -not $MoreOccludersArchive -and -not(Test-Path (Get-SafePath $Root 'r6\scripts\MoreOccluders.reds'))){
        Write-Host 'Optional download: https://www.nexusmods.com/cyberpunk2077/mods/33136?tab=files (Scripted v0.3). Sign in and download manually.'
        $MoreOccludersArchive=(Read-Host 'Downloaded ZIP path (Enter cancels)').Trim('"')
        if(-not $MoreOccludersArchive){return}
    } elseif($interactiveMenu -and -not $MoreOccludersArchive) {
        # An installed script (e.g. the older v0.2) would otherwise always be reused.
        $MoreOccludersArchive=(Read-Host 'MoreOccluders.reds is installed. Path to a newer Scripted ZIP to use instead (Enter keeps the installed script)').Trim('"')
    }
    $stage=Join-Path $CacheRoot ('patch-occluders-'+[guid]::NewGuid().ToString('N'))
    Stage-MoreOccluders250 $Root $stage $MoreOccludersArchive
    $rel='r6\scripts\MoreOccluders.reds';$target=Get-SafePath $Root $rel;$patched=Get-SafePath $stage $rel
    if((Test-Path $target) -and (Get-FileHash $target).Hash -eq (Get-FileHash $patched).Hash){Write-Host 'Already patched; no game files changed.';return}
    if($WhatIf){Write-Host 'Preview only; no game files changed.';return}
    if(-not(Confirm-Change 'Apply optional More Occluders compatibility patch?')){return}
    Assert-GameClosed
    # A separate receipt avoids making this one-file repair a whole-port restore journal.
    $backup=Join-Path $BackupRoot ('optional-occluders-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory $backup -Force|Out-Null
    $existed=Test-Path -LiteralPath $target
    if($existed){Copy-VerifiedFile $target (Join-Path $backup 'MoreOccluders.reds.original')}
    [ordered]@{Target=$target;Existed=$existed;Original='MoreOccluders.reds.original';PatchedSHA256=(Get-FileHash $patched).Hash;Status='Prepared'}|ConvertTo-Json|Set-Content "$backup\receipt.json"
    try { Copy-VerifiedFile $patched $target }
    catch {
        if($existed){Copy-VerifiedFile "$backup\MoreOccluders.reds.original" $target}
        elseif(Test-Path $target){Remove-Item -LiteralPath $target}
        throw
    }
    $receipt=Get-Content "$backup\receipt.json" -Raw|ConvertFrom-Json;$receipt.Status='Installed';$receipt|ConvertTo-Json|Set-Content "$backup\receipt.json"
    Write-Host "Applied optional More Occluders patch. Backup: $backup"
    Write-Host 'To undo: close the game and copy MoreOccluders.reds.original back to r6/scripts/MoreOccluders.reds; if newly installed, remove that script.'
}
