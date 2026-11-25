# 2-BUILD.ps1 -- builds Spirit Tracks from your ROM (home PC).
# The recompiler turns the ROM's code into C (st-recomp\build\<preset>\recomp_generated, never committed), then the
# game (st_game.dll) and the program (SpiritTracks.exe) are compiled. The first build also downloads SDL3.
# -Variant picks the game (st-common.ps1): qol (default, build\release), dpad (build\release-dpad) or clean
# (build\release-clean). Each is its own build: a patched ROM's code is not the clean one's.
#   powershell -NoProfile -ExecutionPolicy Bypass -File 2-BUILD.ps1 [-Clean] [-Jobs N] [-DryRun] [-Variant qol|dpad|clean|eur]
[CmdletBinding()]
param([switch] $Clean, [int] $Jobs = 0, [switch] $DryRun, [ValidateSet('', 'qol', 'dpad', 'clean', 'eur')] [string] $Variant = '')
. (Join-Path $PSScriptRoot 'st-common.ps1')

$rom = Find-Rom

# The source tree: everything the configure step reads.
$need = @('CMakeLists.txt', 'CMakePresets.json', 'third_party\CMakeLists.txt', 'frontend\CMakeLists.txt',
          'frontend\config\bkie\functions.txt', 'frontend\config\bkie\hooks.txt', 'frontend\config\bkie\regions.txt',
          'frontend\config\bkip\functions.txt', 'frontend\config\bkip\hooks.txt', 'frontend\config\bkip1\functions.txt',
          'frontend\config\bkip1\hooks.txt',
          'frontend\src\main.cpp', 'frontend\src\st_hooks.cpp', 'frontend\res\app.ico', 'runtime\include\ntr\recomp.h',
          'recompiler\src\main.cpp')
foreach ($n in $need) { if (-not (Test-Path (Join-Path $Repo $n))) { Fail "missing $Repo\$n" } }
if (-not (Test-Path (Join-Path $Repo 'third_party\melonDS\src\NDS.cpp'))) {
    # (safe.directory: a copy made on another PC belongs to another Windows user, which git refuses by default)
    Run 'fetch the melonDS submodule' { git -c 'safe.directory=*' -C $Repo submodule update --init --depth 1 }
    if (-not $DryRun -and -not (Test-Path (Join-Path $Repo 'third_party\melonDS\src\NDS.cpp'))) {
        Fail 'melonDS is still missing after the fetch: the repository has no submodule link for third_party\melonDS (git ls-files -s third_party/melonDS shows none)'
    }
}
Step "source tree OK"

$missing = @(Get-MissingTools)
if ($missing.Count -gt 0) {
    if ($DryRun) { Step "tools missing (1-INSTALL-TOOLS.ps1 installs them): $($missing -join ', ')" }
    else { Fail "tools missing: $($missing -join ', '). Run 1-INSTALL-TOOLS.ps1 first" }
}
Use-MSVC

$null = Clear-IfMoved $BuildDir
$null = Clear-IfMoved (Join-Path $Repo 'build\gles-test')
if ($Clean -and (Test-Path $BuildDir)) {
    if ($DryRun) { Step "would remove $BuildDir" } else { Step "clean: removing $BuildDir"; Remove-Item -Recurse -Force $BuildDir }
}
Push-Location $Repo
try {
    # SDL's source is fetched once, by the default build; the other variants' builds reuse it when it is there.
    $sdlArgs = @()
    $sdlShared = Join-Path $Repo 'build\release\_deps\sdl3-src'
    if ($BuildPreset -ne 'release' -and (Test-Path (Join-Path $sdlShared 'CMakeLists.txt'))) {
        $sdlArgs = @("-DFETCHCONTENT_SOURCE_DIR_SDL3=$($sdlShared -replace '\\', '/')")
    }
    Run "configure (cmake --preset $BuildPreset)" { cmake --preset $BuildPreset "-DST_ROM=$rom" @sdlArgs }
    $jobArgs = @()
    if ($Jobs -gt 0) { $jobArgs = @('-j', "$Jobs") }
    $t0 = Get-Date
    Run 'build: recompile the ROM, compile the game and the program (10-30+ min the first time)' { cmake --build --preset $BuildPreset @jobArgs }
    if (-not $DryRun) { Step ("build done in {0:N1} min" -f ((Get-Date) - $t0).TotalMinutes) }
    # The phone's GPU renderer on the PC: the program again with the phone's OpenGL ES shaders and shims (the
    # gles-test preset, program only, a few minutes), which 4-TEST.ps1 runs. A failure here does not stop the rest.
    # The program is the same whatever the variant (it loads the game module), so only the default build makes it.
    if ($BuildPreset -eq 'release') {
        $ok = Run-Optional 'gles-test: configure the program with the phone renderer''s shaders' { cmake --preset gles-test }
        if ($ok) { $ok = Run-Optional 'gles-test: build it (a few minutes)' { cmake --build --preset gles-test @jobArgs } }
        if (-not $ok) { Warn "the gles-test program did not build (see $Log); the game itself is fine" }
        elseif (-not $DryRun) { Step 'gles-test program built' }
    } else {
        Step 'gles-test: made by the default (qol) build only'
    }
} finally { Pop-Location }

$exe = Join-Path $BuildDir 'frontend\SpiritTracks.exe'
$dll = Join-Path $BuildDir 'frontend\st_game.dll'
if ($DryRun) { Step "would produce $exe and $dll"; Step 'DRY RUN OK'; exit 0 }
if (-not (Test-Path $exe) -or -not (Test-Path $dll)) { Fail "the build finished but $exe / $dll are missing" }
Step "BUILT: $exe"
exit 0
