#requires -Version 5.1
# Hash identity is not a runtime compatibility guarantee. Never identify by filename alone.
function Get-LoaderCatalog {
    $path = Join-Path (Split-Path -Parent $PSScriptRoot) 'wabbajack\loader-identities.json'
    $catalog = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($catalog.schemaVersion -ne 1) { throw 'Unsupported loader identity catalog. Update the complete switcher package.' }
    foreach ($entry in $catalog.files) {
        if ($entry.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or -not $entry.identity) { throw 'Invalid loader identity catalog.' }
    }
    return @($catalog.files)
}
function Get-LoaderIdentity([string]$Root, [string]$RelativePath, $Catalog) {
    if ($null -eq $Catalog) { $Catalog = @(Get-LoaderCatalog) }
    $path = Join-Path $Root $RelativePath
    $result = [ordered]@{ Path=$RelativePath; State='MISSING'; Identity='Not installed'; Version=$null; Sha256=$null; ExpectedPath=$null }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return [pscustomobject]$result }
    $file = Get-Item -LiteralPath $path
    $result.Version = $file.VersionInfo.FileVersion
    $result.Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    $match = @($Catalog | Where-Object { $_.sha256 -ieq $result.Sha256 } | Select-Object -First 1)
    $result.State = 'UNKNOWN'; $result.Identity = 'Unrecognized DLL (version text alone is not identity)'
    if ($match.Count) {
        $entry = $match[0]
        $result.Identity = $entry.identity; $result.ExpectedPath = $entry.path
        $version = if ($result.Version) { $result.Version.Trim() } else { '' }
        $expected = if ($entry.version) { ([string]$entry.version).Trim() } else { '' }
        if ($version -ne $expected) { $result.State = 'VERSION-MISMATCH' }
        elseif ($RelativePath.Replace('\','/') -ine $entry.path.Replace('\','/')) { $result.State = 'WRONG-LOCATION' }
        else { $result.State = 'VERIFIED' }
    }
    return [pscustomobject]$result
}
function Show-LoaderIdentityReport([string]$Root) {
    $catalog = @(Get-LoaderCatalog)
    Write-Host 'Loader identity checks (SHA-256 + embedded file version):'
    foreach ($rel in @('bin\x64\dxgi.dll','bin\x64\version.dll','bin\x64\winmm.dll','red4ext\RED4ext.dll')) {
        $item = Get-LoaderIdentity $Root $rel $catalog
        $version = if ($item.Version) { $item.Version } else { 'no embedded version' }
        Write-Host "[$($item.State)] $rel : $($item.Identity); $version"
        if ($item.Sha256) { Write-Host "  SHA-256: $($item.Sha256)" }
        if ($item.State -eq 'WRONG-LOCATION') { Write-Warning "Known file is normally at $($item.ExpectedPath). It does not satisfy the loader expected here." }
        if ($item.State -in @('UNKNOWN','VERSION-MISMATCH')) { Write-Warning 'Not verified. This may be a different/newer build, not necessarily a damaged file. Do not delete or rename it based on this message.' }
    }
    Write-Host 'Recognized identity does not prove Cheeky/VR compatibility. Optional dxgi/version loaders are preserved; -DisableDxgiProxy explicitly disables dxgi only.'
    Write-Host 'ASI Loader is not downloaded by this installer. RED4ext requires a verified winmm.dll AND RED4ext.dll pair.'
}
function Test-Red4extIdentity([string]$Root) {
    $catalog = @(Get-LoaderCatalog)
    foreach ($rel in @('bin\x64\winmm.dll','red4ext\RED4ext.dll')) {
        if ((Get-LoaderIdentity $Root $rel $catalog).State -ne 'VERIFIED') { return $false }
    }
    return $true
}
function Assert-Red4extRepairSafe([string]$Root) {
    $catalog = @(Get-LoaderCatalog)
    foreach ($rel in @('bin\x64\winmm.dll','red4ext\RED4ext.dll')) {
        $item = Get-LoaderIdentity $Root $rel $catalog
        if ($item.State -notin @('VERIFIED','MISSING')) {
            throw "Cannot verify $rel as the expected RED4ext component: $($item.State), $($item.Identity), version '$($item.Version)', SHA-256 $($item.Sha256). No game files changed. Existing file preserved; obtain an updated identity catalog for a newer build, or review this file's owner before manually repairing RED4ext. Do not overwrite another mod's loader."
        }
    }
}
