# 6-INSTALL-PHONE.ps1 -- installs the Android app on the phone (USB or wireless debugging) and gives it your game.
#   - installs apks\SpiritTracks-<variant>-latest.apk (keeps the app's data: an update, not a fresh install)
#   - copies the variant's ROM into the app, unless the app has a Spirit Tracks ROM already (clean, D-pad or D-pad + QoL:
#     any of them plays with any build, so one you chose on the phone is kept)
#   - copies a save into the app ONLY if the app has none yet: the PC game folder's (SpiritTracks\data\saves),
#     else your melonDS .ml1.sav. A save already on the phone is never replaced.
#   - opens the app
#   powershell -NoProfile -ExecutionPolicy Bypass -File 6-INSTALL-PHONE.ps1 [-DryRun] [-Variant qol|dpad|clean|eur]
[CmdletBinding()]
param([switch] $DryRun, [ValidateSet('', 'qol', 'dpad', 'clean', 'eur')] [string] $Variant = '')
. (Join-Path $PSScriptRoot 'st-common.ps1')

$apk = Join-Path $ApkDir "SpiritTracks-$Variant-latest.apk"
if (-not (Test-Path $apk)) {
    if ($DryRun) { Step "no APK yet ($apk): 5-BUILD-ANDROID.ps1 makes it" } else { Fail "no APK: run 5-BUILD-ANDROID.ps1 first" }
}
$adb = Find-Adb
if (-not $adb) { Fail "adb not found (the Android SDK in the tools folder has it: $ToolsCache\android-sdk\platform-tools)" }
Step "adb: $adb"

# Runs adb; returns its output (and stops on failure unless -Soft).
function Adb([string[]] $a, [switch] $Soft) {
    if ($DryRun) { Step "would run: adb $($a -join ' ')"; return '' }
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $o = (& $adb @a 2>&1 | ForEach-Object { "$_" }) -join "`n"
    $code = $LASTEXITCODE
    $ErrorActionPreference = $old
    Add-Content -Path $Log -Value "adb $($a -join ' ') -> $code`n$o"
    if ($code -ne 0 -and -not $Soft) { Fail "adb $($a -join ' ') failed: $o" }
    return $o
}

if (-not $DryRun) {
    $devices = @((Adb @('devices')) -split "`n" | Where-Object { $_ -match "`tdevice$" })
    if ($devices.Count -eq 0) { Fail 'no phone: plug it in (USB debugging on) or pair wireless debugging, then run this again' }
    Step "phone: $($devices[0] -replace "`t.*", '')"
}

Adb @('install', '-r', $apk) | Out-Null
Step 'app installed'

# The app's own files (a debug build lets adb reach them with run-as). Files go through a pipe: run-as cannot read adb's folder.
function In-App([string] $cmd) { return Adb @('shell', "run-as $Package sh -c '$cmd'") -Soft }
# True if the app has this file (not just text in an error message that happens to name it).
function Has-File([string] $path) { return (In-App "test -f $path && echo PRESENT") -match 'PRESENT' }
function Push-Into-App([string] $local, [string] $dest) {
    $tmp = '/data/local/tmp/st-push.bin'
    Adb @('push', $local, $tmp) | Out-Null
    $dir = $dest.Substring(0, $dest.LastIndexOf('/'))
    Adb @('shell', "run-as $Package mkdir -p $dir && cat $tmp | run-as $Package sh -c 'cat > $dest'; rm -f $tmp") | Out-Null
}

$rom = Find-Rom
# Any supported ROM plays with any build (known_roms.cpp): the app's own is kept, else this variant's is copied in.
$romSha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath $rom -ErrorAction SilentlyContinue).Hash
$onPhone = if ($DryRun) { '' } else { "$(In-App 'sha1sum files/rom/bkie.nds')" }
$kept = @($PlayableSha1 | Where-Object { $onPhone -match $_ })
if (-not $DryRun -and $romSha1 -and $kept.Count -gt 0) {
    Step "the app has a Spirit Tracks ROM already ($($kept[0])): kept"
} else {
    Push-Into-App $rom 'files/rom/bkie.nds'
    Step "ROM copied into the app ($($V.Label))"
}

$pcSave = Join-Path $GameDir 'data\saves\bkie.sav'
$from = if (Test-Path $pcSave) { $pcSave } elseif (Test-Path -LiteralPath $MelonSave) { $MelonSave } else { $null }
if (-not $DryRun -and (Has-File 'files/saves/bkie.sav')) {
    Step 'the app has a save already: kept (never replaced)'
} elseif ($from) {
    Push-Into-App $from 'files/saves/bkie.sav'
    Step "save copied into the app from $from"
} else {
    Step 'no save to copy: the game starts with a new one'
}

Adb @('shell', 'am', 'start', '-n', "$Package/.LauncherActivity") -Soft | Out-Null
Step 'DONE: the app is open on the phone (Play)'
exit 0
