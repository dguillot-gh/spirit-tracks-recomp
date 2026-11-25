// An analog stick for Spirit Tracks, like the 3DS's circle pad: Link walks the way it is pushed, slowly near the
// middle and running at the rim.
//
// It drives the movement of the D-pad patch (d0k3 / Greiga Master, v0.9c), which every build made from a patched ROM
// carries, with any of the three ROMs (known_roms.cpp). When no stylus is down, the game's touch movement
// (UnkStruct_027e0ce0_40_Base_14::func_ov017_020be44c, zeldaret/st) ends in the patch's routine at 0x02044B28: for
// a held D-pad direction it writes the distance of an imagined stylus from Link (mUnk_3C, fx32 pixels: 96 for Y's
// walk, 1520 to run) and its direction (mUnk_44 and mUnk_46: the camera's yaw plus 0 down, 0x4000 right, 0x8000
// up, 0xC000 left), else stops Link. The hook (st_stick_move, hooks.txt) writes the same from the stick: any
// direction, and a distance that grows with how far the stick is pushed. The stick also holds the D-pad direction it
// points to (main.cpp, stick_dpad): the patch's 0x02044CB8 puts Link in his moving state only for a held D-pad, and
// without it he barely moves. So it acts exactly where the patch's D-pad does (not in cutscenes, menus or talk). A
// build from a clean ROM has no 0x02044B28: there st_pad_move (st_pad.cpp) calls this at the same place.
#pragma once

#include <cstdint>

#include "ntr/recomp.h"

namespace st {

// The stick: x, y in -1..1 (y down), 0, 0 at rest or off. Read by the hook on the game's thread.
void set_stick(float x, float y);

// The D-pad direction (ntr::Key* bits, 8 ways) the stick points to, 0 inside its dead zone.
uint32_t stick_dpad(float x, float y);

}  // namespace st

// Main 0x02044B28 (the D-pad patch's movement), `instead`: 1 when the stick moved Link this frame, 0 to let the
// patch's own code run (the stick at rest, or Select held, which the patch keeps for itself).
extern "C" int st_stick_move(NtrCpu* c);
