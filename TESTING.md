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
