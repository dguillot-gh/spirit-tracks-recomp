# Spirit Tracks port: notes

Started 2026-10-12 as a copy of [phantom-hourglass-recomp](https://github.com/dguillot-gh/phantom-hourglass-recomp)
(its state at e206c39, fresh history): the same FFCCEOT-Recomp engine (recompiler, runtime, melonDS glue; GPL-3.0,
credited in NOTICE.md), the same program, menu, save states and Android app, with the Phantom Hourglass parts swapped
for Spirit Tracks's. Build on the home PC with `scripts\RUN-ALL.ps1` (see `scripts\README.md`).

## What changed from Phantom Hourglass

| Part | Spirit Tracks |
|---|---|
| Game code / config | BKIE (USA), `frontend/config/bkie/` |
| `functions.txt` | 32,522 functions from zeldaret/st (`config/usa/arm9`, commit 1f284ce) by `tools/import_dsd.py`: main 2,153, ITCM 227, 114 overlays (ov097 and ov100 have none); 42 DS Protect functions flagged `!dsprot` |
| `hooks.txt` | OS_Save/Load/InitContext, OS_IrqHandler (below), the six DSProt_* wrappers, the D-pad patch's movement (0x02044B28) for the analog stick, and the native D-pad controls (below) |
| `st_hooks.cpp` | OS_IRQTable / ThreadSwitch addresses; `st_dsprot_detect` (r0 = 0) and `st_dsprot_detect_not` (calls the callback, r0 = 1) |
| ROMs | clean USA, the D-pad patch (`6a63d887…`) and the D-pad + QoL patch (`6896c1db…`) by SHA-1 (`common/src/known_roms.cpp`); one build plays all three (below) |
| Engine | `NTR_ABI_VERSION` 2: `NtrGameInfo.rom_sha1`, the SHA-1 of the ROM the module was recompiled from; the program takes any listed BKIE ROM with any listed module (below) |
| Save | 8 Mbit flash (1 MB): melonDS's ROM list gives BKIE save type 7; the launcher imports 1 MB saves |
| Names | SpiritTracks.exe, st_game.dll / libst_game.so, com.strecomp.st (StInput, StMenu), ST_* CMake variables, logcat tag strecomp |
| Scripts | `-Variant qol|dpad|clean` (presets release / release-dpad / release-clean, game folders SpiritTracks / -DPad / -Clean), logs in `logs\spirit-tracks`, patched ROMs in `st-roms\`, APKs `SpiritTracks-<variant>-*.apk` |
| Icon | railway tracks (`tools/gen_icon.py`) |

## Addresses (BKIE)

The decomp names the NitroSDK OS functions only `OS_func_00nn`. Matched to Phantom Hourglass's (AZEE, same SDK), where
they were identified by their instructions:

| Function | BKIE | AZEE | How |
|---|---|---|---|
| OS_InitContext | main 0x0202708C (0x6C) `OS_func_0062` | 0x0200DC98 | the only run of three functions sized 0x6C, 0x4C, 0x44 in main, in that order |
| OS_SaveContext | main 0x020270F8 (0x4C) `OS_func_0063` | 0x0200DD04 | " |
| OS_LoadContext | main 0x02027144 (0x44) `OS_func_0064` | 0x0200DD50 | " |
| OS_IrqHandler | ITCM 0x01FFBD70 (0x58) | 0x01FFA48C | the only ITCM function of 0x58 followed by one of 0x15C; its last two words are relocations to DTCM 0x027E0198 and ITCM 0x01FFBDC8 (AZEE: 0x027E0014, 0x01FFA4E4) |
| OS_IRQTable | DTCM 0x027E0198 | 0x027E0014 | the literal above |
| OS_IrqHandler_ThreadSwitch | ITCM 0x01FFBDC8 (0x15C) | 0x01FFA4E4 | the literal above (the LR OS_IrqHandler sets) |

**To check on the first build** (the ROM was not on the work PC): `ntr-recomp`'s report and a disassembly of these
five functions should show PH's patterns (`add r0,#0x48` / CP_SaveContext in OS_SaveContext; `mov ip,#0x4000000;
add ip,#0x210` in OS_IrqHandler). A wrong address shows up as a hang right after the logos.

## DS Protect

Overlay 1 carries DS Protect (`libs/dsprot` in the decomp): DetectFlashcart / DetectEmulator / DetectDummy and their
"Not" twins, each wrapped by DSProt_* that decrypts the check, runs it and encrypts it again; the wrappers themselves are
decoded at static-init time. All 42 functions with a `dsprot` property are flagged `!dsprot`: the recompiler emits a
stub for them instead of translating encrypted bytes. The six DSProt_* wrappers are hooked (`hooks.txt`) to answer as a
genuine cartridge: Detect<X> returns 0 without calling the callback, DetectNot<X> calls the callback and returns 1
(dsprotMain's EXPECT_FALSE / EXPECT_TRUE results with every check passing). FFCC (the engine's first game) did the same
with `ntr_dsprot_not_detected`.

## The patched ROMs

d0k3's *DPAD & QoL Hack v0.9c* (USA BPS, source CRC-32 38392357 = the clean dump). The patched ROM is a rebuilt, trimmed
file (94 MB): the header, the ARM9 (patch actions from 0x4000 on, and long target-only runs from 0x6AA42 on: a
recompressed ARM9) and the file layout all change. The decomp's config describes the clean game, so:

- the clean variant is the reference (`RUN-ALL.ps1 -Variant clean`);
- for the patched ones, check on the first build that the recompiler's report has no large rise in undefined
  instructions or discovered functions compared with the clean build, and that new hack code (if it sits outside the
  listed functions) is found (`recomp_report.txt` in each build folder). If needed, add the hack's functions to a
  variant-specific list (the recompiler's discovery already follows BL targets).

Checked 2026-11-22 (clean, D-pad and QoL builds): the three `recomp_report.txt` are identical, and nothing moves:

- decompressed ARM9: the patches differ from the clean dump only at 0x02000B78 (2 bytes) and 0x02044B00..0x02044D78
  (the D-pad hack's 12 routines, outside every listed function; they read nothing of their own bytes as data);
- overlays: 7 functions changed in code (branches into the hack; 73 more differ only in a name comment), no function
  start moved, the overlay table (addresses, sizes, probe words) identical;
- files: the same contents by path (the trimmed ROM numbers them from 183 on one lower; the game opens them by path);
- D-pad vs D-pad + QoL: the same ARM9 and files: the QoL hack lives in the ARM7 (160,048 -> 161,452 bytes), which
  melonDS runs from the ROM.

So a module made from a patched ROM plays the clean dump (with the D-pad controls; without QoL) and both patched ROMs;
one made from the clean dump should play the patched ROMs without the D-pad controls (not tested).
main.cpp and the Android launcher take any listed BKIE ROM; the scripts still build one variant at a time.

## The analog stick

zeldaret/st has no PlayerControl (Phantom Hourglass's stick reads Link's touch state there). The touch movement is
`UnkStruct_027e0ce0_40_Base_14::func_ov017_020be44c` (data_027e0ce0 -> mUnk_40 -> mUnk_014): Link's screen position
is only on its stack, but with no stylus down it ends in the D-pad patch's 0x02044B28, which writes mUnk_3C (the
stylus's distance from Link, fx32 pixels: 0x60000 = 96 Y-walk, 0x5F0000 = 1520 run) and mUnk_44/46 (camera yaw from
0x02078678 with r0 = r8, plus 0 down, 0x4000 right, 0x8000 up, 0xC000 left: its table at 0x02044BA4), and clears
mUnk_40/5C. `st_stick_move` (hooks.txt, `instead`) writes the same from the stick.

## The D-pad controls, native (2026-11-25)

The D-pad patch's ARM9 part changes 10 places in the clean code (found by diffing the dsd-extracted clean and D-pad
ROMs): 7 calls or instructions in overlays 0, 17, 24, 93 and 102 now branch into its 12 routines. `frontend/src/st_pad.cpp`
does each routine's work by a hook at the same place, so a module built from the clean dump (or a European one) has the
controls: movement (Link's stop call, `st_pad_move`, which also takes the stick), B slashes (`st_pad_action`), A taps
in front of Link (`st_pad_a_tap`), A + direction rolls (`st_pad_roll`), A held as a held stylus (`st_pad_a_hold`), B + Y
(`st_pad_by`), and AdventureModeManager's Right / Down / Select moved to Select + Right / Down / Up and overlay 93's
A / Left to Select + Left (`st_pad_keys_*`).

Two recompiler features made that possible: an `at` hook (a C function called just before one instruction, in every
function that holds it) and an expected instruction word after any hook (`=e3100010`): where the code differs (a module
built from a patched ROM) the hook is left out with a warning in the report, so the patch's own code runs. The three
call-site hooks also check the return address (lr), which in a patched module is the patch's.
Checked on a Pixel 8 (2026-11-25, clean USA build): the D-pad and the stick move, and A / B / rolls work; the QoL build
leaves the 7 patched-site hooks out. The European addresses are mapped by the decomp's function names, untested.

## Widescreen (fixed 2026-11-22)

Phantom Hourglass builds its projection (MTX_PerspectiveW, 0x02022BCC here) every frame, and the hook there widens
it and keeps the scene marked wide (ffcc_wide_scene counts down from 8 per frame). Spirit Tracks builds it only when
the camera's field of view changes and sets the viewport once per scene, so 16:9 and 21:9 showed a 4:3 picture pinned
to the left: after 8 frames the scene counted as 4:3, and a scene continued from a save state (every widescreen change
restarts from one) kept its 4:3 matrix and viewport. Now:

- `st_load_mtx44` (hooks.txt, G3_LoadMtx44 0x020249EC, `before`): a perspective matrix whose aspect is one of ours
  gets the chosen one (also back to 4:3), in the game's stored copy, and a wide one keeps the scene marked wide;
- third_party/CMakeLists.txt (GPU3D::VBlank): a full-screen viewport follows the scene's width every frame (it was
  only set when the game sent VIEWPORT).

Checked on the phone from a continued save state: 16:9 and 21:9 fill the screen with the HUD at the edges, 4:3 is
back to normal.

## GPU dots (fixed by Compute)

On a Pixel 8 (Mali-G715, OpenGL ES 3.2) melonDS's OpenGL renderer showed single cyan pixels along polygon edges (Link's
house): the game's fog colour (DS 19,31,29) on pixels fogged as if at the far plane. The software renderer, the PC's
OpenGL renderer and Phantom Hourglass on the same phone were clean. The compute renderer (ported from PH, 2026-10-28) has
none; the owner confirmed it on the phone. The menus now offer Software / Compute only; the OpenGL renderer draws only
when a phone cannot run Compute (or with `--renderer opengl`). The temporary debug switch (09ca021) is removed.

## History

- 2026-10-12: project created from ph-recomp; config imported from zeldaret/st; hooks matched; DS Protect hooks;
  variants and BPS patcher; module bound to its ROM. Dry runs of every script OK on the work PC; C++ (PC + NDK) and Java
  syntax-checked. Not built yet.

- 2026-10-28: Phantom Hourglass's engine graphics work ported (714974c): the compute renderer on the phone (OpenGL ES
  3.2, `third_party/gles_compute_fixes.cmake`, `tools/check_gles_compute.py`), Renderer Software / GPU / Compute in both
  menus (phone Compute up to 4x), texture wrap modes through sampler objects, the GL context recovered after the app
  returns from the background; wireless removed as in PH. The blow sample (`runtime/src/mic_sample_data.hpp`, the
  owner's "PERFECT BLOW FOR DS EMULATORS.wav", as in PH): N on PC, the BLOW button on the phone, for the Spirit Flute.
  The start screen's picture is drawn in code (`LauncherArt.java`, our own drawing). 5-BUILD-ANDROID uses the Android
  SDK's newest CMake (3.28+) when none is installed (cmake;3.31.6 added to the drive's SDK).

- 2026-10-28 (later): Phantom Hourglass's frontend features merged in (three-way merge of PH e206c39 -> main):
  Continue (resume state on quit / background), the phone's layout editor (move and resize controls, per orientation),
  widescreen 16:9 / 21:9 (hook on G3i_PerspectiveW_ at main 0x02022bcc, matched to PH's 0x02005bfc: same arguments,
  the hardware divider, the same scaling; HUD clusters at the wide edges), the 3D textures and 3D fixes options,
  landscape launcher layout, visible menu switches. Not ported: the analog stick (it reads PH's gPlayerControl; Spirit
  Tracks's touch state is not found yet: `frontend/src/st_stick.cpp` is off and the menus hide it) and PH's
  `hud_sides` (PH's own HUD). Widescreen needs the generated C made again (2-BUILD: hooks.txt changed); an APK built
  from the old generated C draws 16:9 without widening the 3D. (On the work PC, its security software deleted
  build
elease
ecompiler
tr-recomp.exe when it was run; 2-BUILD at home builds it again.)

## Next

1. First home build: `RUN-ALL.ps1` (D-pad + QoL), then `RUN-ALL.ps1 -Variant clean`; compare both `recomp_report.txt`.
2. (Done 2026-11-22: the D-pad-only ROM's SHA-1 is listed.)
3. Android: `RUN-ANDROID.ps1`.
4. Screenshots for the README.
