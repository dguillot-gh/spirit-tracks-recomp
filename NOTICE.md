# Notices

ST-Recomp is a native PC and Android port of *The Legend of Zelda: Spirit Tracks* (Nintendo DS) by static recompilation. It
contains **no** game code or data: the game is recompiled on your PC from your own copy of the ROM. *The Legend of
Zelda: Spirit Tracks* is © Nintendo. This project is not affiliated with or endorsed by Nintendo.

## Where the code comes from

| Part | Origin | Licence |
|---|---|---|
| `frontend/` (the program, its window, input, saves, icon), `frontend/config/bkie`, `android/`, `tools/gen_icon.py`, `tools/bps_patch.py` | written for this project (started as a copy of [phantom-hourglass-recomp](https://github.com/dguillot-gh/phantom-hourglass-recomp), the same author's port of Phantom Hourglass) | GPL-3.0 |
| `recompiler/`, `runtime/`, `common/`, `cmake/`, `third_party/CMakeLists.txt`, `third_party/compat`, the `tools/` they use | [FFCCEOT-Recomp](https://github.com/JonathanDC64/FFCCEOT-Recomp) by JonathanDC64: its game-agnostic recompiler (`ntr-recomp`) and DS runtime (`ntr-runtime`), used as the engine of this fork with small changes (Spirit Tracks ROM list, microphone input, the module records the ROM it was built from) | GPL-3.0 |
| `third_party/melonDS` | [melonDS](https://github.com/melonDS-emu/melonDS) | GPL-3.0 |
| SDL3 (fetched at build time) | [SDL](https://libsdl.org) | zlib |
| `frontend/src/font8x8_basic.h` (the menu's text) | [font8x8](https://github.com/dhepper/font8x8) by Daniel Hepper, after the IBM VGA fonts | public domain |
| `frontend/src/st_text.cpp`, `st_text.hpp`, the HD text parts of `gl_present.cpp`, `main.cpp`, the menus and `cmake/EmbedFont.cmake` | ported from [phantom-hourglass-recomp](https://github.com/dguillot-gh/phantom-hourglass-recomp) PR #7 by [chouette-cc](https://github.com/chouette-cc) (HD vector dialogue text), with Spirit Tracks's own hooks | GPL-3.0 |
| The language selector in `main.cpp`, `menu.cpp`/`menu.hpp`, `GameActivity.java`, `LauncherLook.java` and `StMenu.java` | ported from [phantom-hourglass-recomp](https://github.com/dguillot-gh/phantom-hourglass-recomp) PR #6 by [chouette-cc](https://github.com/chouette-cc) (firmware language selector), limited to the USA ROM's English, French and Spanish | GPL-3.0 |
| European ROM support: `frontend/config/bkip*`, `st_hooks.cpp`'s per-release addresses, `frontend/CMakeLists.txt`, `known_roms.cpp`, the launcher and the scripts' `eur` variant | modelled on [phantom-hourglass-recomp](https://github.com/dguillot-gh/phantom-hourglass-recomp) PRs #8 and #6 by [chouette-cc](https://github.com/chouette-cc) (Europe ROM, German and Italian); Spirit Tracks's addresses mapped from the [zeldaret/st](https://github.com/zeldaret/st) decomp's `config/eur` and `config/eur1` | GPL-3.0 |
| `third_party/stb/stb_truetype.h` (TrueType rasterizer for HD text) | [stb](https://github.com/nothings/stb) by Sean Barrett | MIT / public domain |
| `frontend/res/fonts/Inter-Bold.ttf` (optional HD dialogue font) | [Inter](https://github.com/rsms/inter) by Rasmus Andersson | SIL OFL 1.1 (`frontend/res/fonts/Inter-OFL.txt`) |
| Function addresses and names in `frontend/config/bkie/functions.txt` | the [zeldaret/st](https://github.com/zeldaret/st) decompilation project's dsd configuration (`config/usa/arm9`) | facts only (addresses, sizes, names) |

The optional D-pad and D-pad + QoL patches (*DPAD & QoL Hack* v0.9c by d0k3, with the D-Pad Patch by Greiga Master /
StraDaMa) are not part of this project either: the build scripts apply your own copy of the BPS files to your own ROM,
on your PC, with `tools/bps_patch.py`.

FFCCEOT-Recomp's own frontend, menus, art, fonts, theme and game layer (Final Fantasy Crystal Chronicles specific) are
not part of this project. The whole program is GPL-3.0 (`LICENSE`).
