# Testing: devices, measurements and known issues

## Devices

- **Moto G Stylus 2025** (Snapdragon 6 Gen 3, Adreno 710, Android 16, 120 Hz display): the main test phone since
  2026-11-14.
- **Google Pixel 8** (Tensor G3, Mali-G715, Android 14+) and **LG V30** (Snapdragon 835, Adreno 540, Android 8, rooted
  for measuring): measured on 2026-11-03.
- **PC:** Windows 11.

Other phones should work (Android 8 or newer, 64-bit ARM), but GPU drivers differ: if the picture looks wrong, the
software renderer (1x, in the menu) is the safe fallback.

## Checked on the Moto G Stylus 2025 (2026-11-22 and 23)

- **ROMs:** one build (from the D-pad + QoL ROM) boots and plays the clean dump and the QoL ROM. With the clean dump the
  D-pad controls work; the QoL changes do not (they are in the QoL ROM's ARM7). A save state made with one ROM does not
  load with another (the in-game save does).
- **Analog stick:** full push runs as far as the D-pad in the same time, a light push walks, Y walks slowly.
- **Widescreen:** cutscenes fill the wide picture (letterbox bars only above and below).
- **Widescreen:** 16:10, 16:9 and 21:9 fill the screen after Continue and after switching modes; 4:3 is back to normal after.
  The HUD stays at the edges under dialogue boxes, the horn cord stays at the edge pulled or not, the Menu bar opens at
  the right edge with its button.
- **HD text:** file select, dialogue pages, choices, place-name cards; the text comes back after Continue (saved beside
  the state).
- **Minimap:** shows the map in the corner, moves above dialogue boxes, hides in cutscenes, drags with a long press,
  hides and shows with the MAP button. (Pinch to resize: not checked with two fingers yet.)
- **Frame rate:** the game runs at 59.7-59.8 fps (its own counter). With the 120 Hz display mode Android no longer caps
  the app at 45 fps on the panel's 90 Hz mode, and with the performance hint the game thread's cores run at about
  2.0-2.2 GHz instead of about 1.5. SurfaceFlinger counted 40-45 frames shown per second in a quiet scene at
  Compute 2x; whether that is real misses or repeated frames is not yet known.

## On-device compiler: clang vs TinyCC (2026-11-25, Moto G Stylus 2025 and Pixel 8)

The game built on the phone from the D-pad + QoL ROM, once with TinyCC and once with clang -O2, then run from the
same save states (the resume state and slots 1, 3, 6, 9) with `--unpaced --exit-after-frames 1320 --continue`
(frames 120 to 1320 timed; the launcher's `bench_frames` extra). Frames per second, unpaced: above 60 is headroom.

**Build time** (the whole game, 65 files / 100 MB of C): Moto G clang 518 s (5 at a time), TinyCC 11.7 s;
Pixel 8 clang 369 s compile, TinyCC 8.7 s. PC, one thread: clang 390 s.

| Phone | Renderer | Save spot | TinyCC | clang |
|---|---|---|---|---|
| Moto G | Compute x2 | resume | 54.3* | 76.3 |
| Moto G | Compute x2 | slot 1 | 67.0 | 68.3 |
| Moto G | Compute x2 | slot 3 | 89.2 | 90.3 |
| Moto G | Compute x2 | slot 6 | 66.6 | 67.7 |
| Moto G | Compute x2 | slot 9 | 66.5 | 67.3 |
| Moto G | OpenGL x2 | resume | 77.2 | 77.0 |
| Moto G | OpenGL x2 | slot 1 | 85.3 | 85.2 |
| Moto G | OpenGL x2 | slot 3 | 105.1 | 105.3 |
| Moto G | OpenGL x2 | slot 6 | 83.5 | 84.3 |
| Moto G | OpenGL x2 | slot 9 | 81.6 | 80.5 |
| Moto G | Software x1 | resume | 97.0 | 95.7 |
| Moto G | Software x1 | slot 1 | 100.4 | 99.9 |
| Moto G | Software x1 | slot 3 | 99.0 | 98.9 |
| Moto G | Software x1 | slot 6 | 93.5 | 93.2 |
| Moto G | Software x1 | slot 9 | 101.0 | 100.0 |
| Pixel 8 | Compute x2 | resume | 116.5* | 199.1* |
| Pixel 8 | Compute x2 | slot 1 | 113.5 | 113.9 |
| Pixel 8 | Compute x2 | slot 3 | 147.8 | 146.1 |
| Pixel 8 | Compute x2 | slot 6 | 115.0 | 116.7 |
| Pixel 8 | Compute x2 | slot 9 | 103.2 | 103.9 |
| Pixel 8 | OpenGL x2 | resume | 110.0 | 109.9 |
| Pixel 8 | OpenGL x2 | slot 1 | 115.9 | 121.2 |
| Pixel 8 | OpenGL x2 | slot 3 | 143.6 | 153.2 |
| Pixel 8 | OpenGL x2 | slot 6 | 120.5 | 120.7 |
| Pixel 8 | OpenGL x2 | slot 9 | 108.9 | 110.0 |
| Pixel 8 | Software x1 | resume | 99.6 | 99.8 |
| Pixel 8 | Software x1 | slot 1 | 102.3 | 103.0 |
| Pixel 8 | Software x1 | slot 3 | 123.6 | 122.8 |
| Pixel 8 | Software x1 | slot 6 | 99.1 | 97.7 |
| Pixel 8 | Software x1 | slot 9 | 106.8 | 106.7 |

\* The first runs of a series. Repeated three times each (TinyCC first): Moto G TinyCC 54.1, 77.2, 76.5 / clang 78.6,
79.1, 76.1; Pixel 8 TinyCC 116.1, 187.2, 142.5 / clang 193.7, 154.8, 152.1. So the big gap was a first-run effect,
not the compiler: once warm, TinyCC and clang run the same (within about 3%), and the Pixel 8 varies a lot run to run.

**Findings**
- The compiler is not what limits the frame rate: the game code is a small part of the frame. clang is never slower
  and builds a smaller module (55 MB against 89 MB), but costs minutes of build time and a much bigger download.
- The renderer matters more: on the Moto G, OpenGL x2 is 10-25% faster than Compute x2 in most spots.
- The first run after a start can be much slower (54 against 77 fps on the Moto G): worth looking into, it may be a
  hitch players feel (shader compiles, the module's first page-in, CPU clocks ramping up).
- Both phones run every spot above 60 unpaced, at every renderer, with either compiler.

## Measured on the Pixel 8 and the LG V30 (2026-11-03)

Each renderer set the way the menu does, the game continued from the same spot in gameplay, 20 s to settle, then 30 s
of the frames the phone actually showed (Android's SurfaceFlinger: `--latency` on the V30, `--timestats` on the
Pixel 8). Every run started the renderer it asked for (checked in the game's log).

| Renderer | LG V30 fps | V30 1% low | Pixel 8 fps | Pixel 1% low |
|---|---:|---:|---:|---:|
| Software (1x) | 32.0 | 15 | 61.5 | 40 |
| OpenGL 2x | 36.5 | 20 | 61.4 | 30 |
| **OpenGL 4x** | **47.3** | 30 | 61.5 | 30 |
| Compute 2x | 21.3 | 10 | 61.4 | 30 |
| Compute 4x | 18.6 | 8 | 61.5 | 30 |

The Pixel 8 runs every setting at full speed (~60 fps; it measures slightly over 60 on its 120 Hz display). The phone
app caps the resolution at 4x. 1% low = the frame rate of the slowest 1% of frames.

**Best settings:** **OpenGL 4x** on older phones like the V30; **Compute** (the more exact renderer) on fast phones. On
the V30, 4x runs faster than 2x: the heavier load makes the phone raise its CPU and GPU clocks (fast cores ~1.9 GHz and
the GPU at ~670 MHz at 4x, against ~1.2 GHz and 257 MHz at 2x).

## Known issues

- **Continue with a dialogue box open (widescreen):** the train lever and the Menu button stay in the 4:3 middle until
  that box closes; after that they stay at the edges.
- **HD text in older save states:** a state saved before HD text was kept with states brings its boxes back empty
  until their next page.
- **Analog stick on a clean-ROM build** (`-Variant clean`): none yet. A stick for the stock game, without the D-pad
  patch, is in progress.
- **Software renderer:** the bottom of the screen flickers on phones (seen 2026-11-03); OpenGL and Compute are not
  affected. The minimap's cutscene hiding needs a GPU renderer.
- **Mali GPUs (Pixel 8):** the OpenGL renderer showed stray dots along polygon edges; Compute does not (ST-PORT.md).
- **Scenes with 3D on both screens** have not been checked in OpenGL yet.

## For contributors

The unit tests need no ROM: after building, run `ctest --preset release`. `scripts\4-TEST.ps1` also plays the game for
a minute as a smoke test. On a phone, `adb logcat -s strecomp` shows the game's log. Before a pull request that touches
settings, menus or the on-screen controls, check both the PC menu and the Android menu, in landscape and portrait.
