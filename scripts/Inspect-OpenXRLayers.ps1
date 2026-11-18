#requires -Version 5.1
# Registration/file evidence only. Never loads layer DLLs or writes registry/settings.
function Get-CpvrProperty($Object,[string]$Name,$Default=$null) {
    if($null -eq $Object){return $Default}
    $p=$Object.PSObject.Properties[$Name]
    if($null -eq $p){return $Default}
    return $p.Value
}
function Read-CpvrXrRegistrations {
    $rows=New-Object 'System.Collections.Generic.List[object]'
    $errors=New-Object 'System.Collections.Generic.List[string]'
    foreach($hive in @('CurrentUser','LocalMachine')) {
        $base=$null
        try {
            $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::$hive,[Microsoft.Win32.RegistryView]::Registry64)
            foreach($kind in @('Implicit','Explicit')) {
                $sub="SOFTWARE\Khronos\OpenXR\1\ApiLayers\$kind";$key=$null
                $label=if($hive -eq 'CurrentUser'){'HKCU'}else{'HKLM'}
                try {
                    $key=$base.OpenSubKey($sub,$false)
                    if($null -eq $key){continue}
                    $names=@($key.GetValueNames() | Sort-Object)
                    if($names.Count -gt 256){$errors.Add("${label}\${sub}: registration scan capped at 256 entries.")}
                    foreach($manifest in @($names | Select-Object -First 256)) {
                        $rows.Add([pscustomobject]@{Key="$label\$sub";View='Registry64';Kind=$kind;Manifest=$manifest;Value=$key.GetValue($manifest,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);ValueKind=[string]$key.GetValueKind($manifest)})
                    }
                } catch {$errors.Add("${label}\${sub}: $($_.Exception.Message)")} finally {if($key){$key.Dispose()}}
            }
        } catch {$errors.Add("${hive} Registry64: $($_.Exception.Message)")} finally {if($base){$base.Dispose()}}
    }
    [pscustomobject]@{Rows=@($rows.ToArray());Errors=@($errors.ToArray())}
}
function Test-CpvrLocalXrPath([string]$Path) {
    # Do not contact UNC/network paths supplied by a registration or manifest.
    if($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path.Substring(2) -match ':'){return $false}
    try {
        $drive=New-Object IO.DriveInfo($Path.Substring(0,3))
        return $drive.DriveType -ne [IO.DriveType]::Network
    } catch {return $false}
}
function Get-CpvrXrBinary([string]$Path) {
    $r=[ordered]@{Path=$Path;State='MISSING';Version=$null;Sha256=$null;Error=$null}
    try {
        if(-not(Test-CpvrLocalXrPath $Path)){$r.State='NOT-INSPECTED';$r.Error='Only local drive paths are inspected; network/device/relative paths are not opened.';return [pscustomobject]$r}
        $f=Get-Item -LiteralPath $Path -ErrorAction Stop
        if($f.PSIsContainer){throw 'Expected a DLL file, found a directory.'}
        if($f.Length -gt 256MB){$r.State='NOT-INSPECTED';$r.Error='DLL exceeds the 256 MiB diagnostic limit.';return [pscustomobject]$r}
        $r.Version=[Diagnostics.FileVersionInfo]::GetVersionInfo($Path).FileVersion
        $r.Sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256 -ErrorAction Stop).Hash
        $r.State='READ'
    } catch [System.Management.Automation.ItemNotFoundException] {} catch {$r.State='UNREADABLE';$r.Error=$_.Exception.Message}
    [pscustomobject]$r
}
function Get-CpvrXrLayer($Registration) {
    $r=[ordered]@{Key=$Registration.Key;View=$Registration.View;Kind=$Registration.Kind;Manifest=$Registration.Manifest;Value=$Registration.Value;ValueKind=$Registration.ValueKind;Registration='INVALID';State='MISSING-MANIFEST';Name=$null;ImplementationVersion=$null;DisableEnvironment=$null;EnableEnvironment=$null;Binary=$null;Error=$null;ManifestSha256=$null}
    if($Registration.ValueKind -eq 'DWord') {
        if($Registration.Value -ne 0){$r.Registration='DISABLED'}
        elseif($Registration.Kind -eq 'Implicit'){$r.Registration='ENABLED-REGISTRATION'}
        else {$r.Registration='EXPLICIT-AVAILABLE'}
    }
    try {
        if(-not(Test-CpvrLocalXrPath $Registration.Manifest)){$r.State='NOT-INSPECTED';$r.Error='Manifest is not a local absolute drive path.';return [pscustomobject]$r}
        $f=Get-Item -LiteralPath $Registration.Manifest -ErrorAction Stop
        if($f.PSIsContainer -or $f.Length -gt 1MB){throw 'Expected a manifest file of at most 1 MiB.'}
        $r.State='INVALID-MANIFEST'
        $json=Get-Content -LiteralPath $f.FullName -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
        $layer=Get-CpvrProperty $json 'api_layer'
        $r.Name=Get-CpvrProperty $layer 'name';$lib=Get-CpvrProperty $layer 'library_path'
        if($r.Name -isnot [string] -or [string]::IsNullOrWhiteSpace($r.Name) -or $lib -isnot [string] -or [string]::IsNullOrWhiteSpace($lib)){throw 'Manifest needs api_layer.name and library_path strings.'}
        $r.ImplementationVersion=Get-CpvrProperty $layer 'implementation_version'
        foreach($field in @('disable_environment','enable_environment')) {
            $val=Get-CpvrProperty $layer $field
            if($null -ne $val -and ($val -isnot [string] -or $val -notmatch '^[A-Za-z_][A-Za-z0-9_]{0,127}$')){throw "Invalid $field name."}
            if($field -eq 'disable_environment'){$r.DisableEnvironment=$val}else{$r.EnableEnvironment=$val}
        }
        $r.ManifestSha256=(Get-FileHash -LiteralPath $f.FullName -ErrorAction Stop).Hash
        $dll=if([IO.Path]::IsPathRooted($lib)){$lib}else{[IO.Path]::GetFullPath((Join-Path $f.DirectoryName $lib))}
        $r.Binary=Get-CpvrXrBinary $dll
        $r.State=if($r.Binary.State -eq 'READ'){'READ'}elseif($r.Binary.State -eq 'MISSING'){'MISSING-DLL'}else{$r.Binary.State}
    } catch [System.Management.Automation.ItemNotFoundException] {} catch {$r.Error=$_.Exception.Message;if($r.State -ne 'INVALID-MANIFEST'){$r.State='UNREADABLE'}}
    [pscustomobject]$r
}
function Get-CpvrOpenXrDiagnostics {
    param([string]$Root,$RegistrationSnapshot,$EnvironmentSnapshot)
    if(-not $PSBoundParameters.ContainsKey('RegistrationSnapshot')){$RegistrationSnapshot=Read-CpvrXrRegistrations}
    $layers=@($RegistrationSnapshot.Rows | ForEach-Object {Get-CpvrXrLayer $_})
    $names=@('CHEEKY_OPENXR_LAYER_DISABLE','XR_API_LAYER_PATH','XR_ADD_API_LAYER_PATH','XR_ENABLE_API_LAYERS','XR_LOADER_LAYERS_ENABLE','XR_LOADER_LAYERS_DISABLE','XR_LOADER_LAYERS_ALLOW','XR_RUNTIME_JSON')
    foreach($l in $layers){if($l.DisableEnvironment){$names+=$l.DisableEnvironment};if($l.EnableEnvironment){$names+=$l.EnableEnvironment}}
    $envRows=@();$errors=@($RegistrationSnapshot.Errors)
    if($PSBoundParameters.ContainsKey('EnvironmentSnapshot')){$envRows=@($EnvironmentSnapshot)}
    else {
        foreach($scope in @('Process','User','Machine')) {
            foreach($name in @($names | Sort-Object -Unique)) {
                try {$value=[Environment]::GetEnvironmentVariable($name,[EnvironmentVariableTarget]::$scope)
                    if($null -ne $value){$envRows+=[pscustomobject]@{Scope=$scope;Name=$name;Value=$value}}
                } catch {$errors+="Environment ${scope}/${name}: $($_.Exception.Message)"}
            }
        }
    }
    $hints=New-Object 'System.Collections.Generic.List[string]'
    $eligible=@($layers | Where-Object Registration -eq 'ENABLED-REGISTRATION')
    foreach($group in @($eligible | Where-Object {$_.Name} | Group-Object Name)) {
        if($group.Count -gt 1) {
            $paths=@($group.Group | ForEach-Object {[IO.Path]::GetFullPath($_.Manifest).ToLowerInvariant()} | Select-Object -Unique)
            if($paths.Count -gt 1){$hints.Add("POSSIBLE DUPLICATE LAYER: $($group.Name) has $($paths.Count) distinct enabled implicit manifest paths. Check stale installations; this does not prove both loaded.")}
            else {$hints.Add("REPEATED REGISTRATION: $($group.Name) appears in multiple registry keys for the same manifest; not evidence of two DLLs loaded.")}
        }
    }
    $cheeky=@($layers | Where-Object {$_.Name -match '(?i)cheeky' -or $_.Manifest -match '(?i)cheeky'})
    $liveCheeky=@($cheeky | Where-Object {$_.Registration -eq 'ENABLED-REGISTRATION' -and $_.State -eq 'READ'})
    $cheekyComponents=@()
    foreach($name in @('CheekyFoveatedDLSSHost.dll','CheekyFoveatedDLSSRuntime.dll')){$cheekyComponents+=Get-CpvrXrBinary (Join-Path $Root "bin\x64\CheekyFoveatedDLSS\$name")}
    if(@($cheekyComponents | Where-Object State -eq 'READ').Count -and -not $liveCheeky.Count){$hints.Add('NO READABLE ENABLED CHEEKY REGISTRATION: game Host/Runtime found, but no readable enabled implicit Cheeky layer in the 64-bit registry scan. Environment paths/explicit loading may differ; not proof the layer cannot load.')}
    $versions=@(@($liveCheeky | ForEach-Object {$_.Binary}) + $cheekyComponents | Where-Object {$_.State -eq 'READ' -and $_.Version} | ForEach-Object {$_.Version.Trim()} | Select-Object -Unique)
    if($versions.Count -gt 1){$hints.Add('DIFFERENT CHEEKY VERSION LABELS: layer/Host/Runtime may come from different packages. Compare release provenance and hashes; labels alone do not prove incompatibility. Equal labels also do not prove a match.')}
    if(@($eligible | Where-Object {$_.Name -match '(?i)OFXR|XRFrameBridge'}).Count -and $liveCheeky.Count){$hints.Add('CHEEKY + OFXR registrations present. Coexistence is not automatically a conflict; verify matched Cheeky components and intended framegen ownership using the same-run logs. A closed tray alone does not establish layer state.')}
    if($envRows.Count){$hints.Add('ENVIRONMENT OVERRIDES PRESENT: inspect the values below. Process means this diagnostic process, not the running game. Steam may retain older inherited values until restarted. Extra layer directories are reported, not recursively scanned.')}
    if($errors.Count){$hints.Add('INCOMPLETE SCAN: access/read errors below must be resolved before concluding a layer is absent.')}
    $hints.Add('Layer name and implementation_version are manifest declarations, not release identification. Disk hashes identify files, not the DLL loaded in an existing game process. No upstream/fork origin inferred from names alone.')
    $hints.Add('DECONFLICT: close VR applications; install matching Cheeky game files and OpenXR setup from one release. Back up and disable only confirmed stale registrations; retain the matching layer for gaze/stereo calibration. This report makes no changes.')
    [pscustomobject]@{Layers=$layers;Environment=$envRows;Errors=$errors;Components=$cheekyComponents;Hints=@($hints.ToArray())}
}
function Format-CpvrOpenXrDiagnostics($Report) {
    ''; 'OPENXR LAYERS (64-bit HKCU/HKLM, implicit + explicit; read-only)'
    '0 = enabled implicit registration / available explicit layer; nonzero DWORD = disabled. Registration is NOT proof of loading.'
    if(-not $Report.Layers.Count){'No registrations returned; check scan errors and environment overrides below.'}
    foreach($l in $Report.Layers) {
        "[$($l.Registration) / $($l.State)] $($l.Name)"
        "  $($l.Key) [$($l.View)] value=$($l.Value) type=$($l.ValueKind)"
        "  Manifest: $($l.Manifest)"
        if($l.ManifestSha256){"  Manifest SHA-256: $($l.ManifestSha256)"}
        if($l.ImplementationVersion){"  Declared implementation_version: $($l.ImplementationVersion) (not package version)"}
        if($l.DisableEnvironment){"  disable_environment: $($l.DisableEnvironment)"}
        if($l.EnableEnvironment){"  enable_environment: $($l.EnableEnvironment)"}
        if($l.Error){"  Read error: $($l.Error)"}
        if($l.Binary){"  DLL [$($l.Binary.State)]: $($l.Binary.Path); version=$($l.Binary.Version)";if($l.Binary.Sha256){"  DLL SHA-256: $($l.Binary.Sha256)"};if($l.Binary.Error){"  DLL read error: $($l.Binary.Error)"}}
    }
    foreach($c in $Report.Components){"Cheeky component [$($c.State)]: $($c.Path); version=$($c.Version); SHA-256=$($c.Sha256)";if($c.Error){"  $($c.Error)"}}
    foreach($e in $Report.Environment){"Environment [$($e.Scope)] $($e.Name)=$($e.Value)"}
    foreach($e in $Report.Errors){"SCAN ERROR: $e"}
    foreach($hint in $Report.Hints){"NOTE: $hint"}
}
