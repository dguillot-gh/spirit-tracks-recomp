# RUN-ANDROID.ps1 -- the Android app: build the APK (after the PC build, which it runs first if missing), install it.
#   ANDROID-TOOLS.ps1     Java 17 and the Android SDK, once (asks you to accept Android's SDK licence)
#   5-BUILD-ANDROID.ps1   the APK (apks\SpiritTracks-<variant>-latest.apk)
#   6-INSTALL-PHONE.ps1   install, copy the ROM and (only if the phone has none) a save, open the app
#   powershell -NoProfile -ExecutionPolicy Bypass -File RUN-ANDROID.ps1 [-DryRun] [-NoPhone] [-Variant qol|dpad|clean|eur]
[CmdletBinding()]
param([switch] $DryRun, [switch] $NoPhone, [ValidateSet('', 'qol', 'dpad', 'clean', 'eur')] [string] $Variant = '')
. (Join-Path $PSScriptRoot 'st-common.ps1')

$steps = @('ANDROID-TOOLS.ps1')
# The APK is built from the PC build's generated C: make it first if this variant has none yet.
if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'recomp_generated\func_table.c'))) {
    Step "no PC build of this variant in $BuildDir yet: running 1-INSTALL-TOOLS.ps1 and 2-BUILD.ps1 first"
    $steps += '1-INSTALL-TOOLS.ps1', '2-BUILD.ps1'
}
$steps += '5-BUILD-ANDROID.ps1'
if (-not $NoPhone) { $steps += '6-INSTALL-PHONE.ps1' }
foreach ($s in $steps) {
    Step "---- $s"
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot $s) -DryRun:$DryRun -Variant $Variant
    if ($LASTEXITCODE -ne 0) { Fail "$s failed (see logs\progress.log)" }
}
if ($DryRun) { Step 'DRY RUN OK' } else { Step 'ANDROID DONE' }
