// Spirit Tracks's own code that the recompiled game calls: the hooks of frontend/config/<code>/hooks.txt.
#pragma once

#include <cstdint>
#include <string>

namespace st {

// The game's own addresses the frontend uses, per release (zeldaret/st config/usa, eur, eur1).
struct Addresses {
    uint32_t os_irq_table;                  // OS_IRQTable (DTCM): the literal OS_IrqHandler loads
    uint32_t os_irq_handler_thread_switch;  // OS_IrqHandler_ThreadSwitch (ITCM): the LR OS_IrqHandler sets
    uint32_t oam_mgr_a, oam_mgr_b;          // the two OAM managers func_02009b18 is called with (st_text.cpp)
    uint32_t input_ptr;                     // the game's input (st_stick.cpp; the D-pad patch, USA only)
    uint32_t camera_yaw;                    // UnkStruct_027e09bc_0C::func_ov000_02078698 (st_stick.cpp)
    uint32_t camera_planes;                 // func_ov000_020764c0: the camera's view planes (st_camera_planes)
};
const Addresses& addresses();

// Picks the addresses for the release with this game code and ROM revision ("BKIE": USA; "BKIP" rev 0 or 1:
// Europe). False for any other.
bool select_release(const std::string& game_code, int revision = 0);

// A hook of hooks.txt by name, for the game module; null if there is none of that name.
void* find_hook(const char* name);

// Widescreen: the 3D drawn `extra` DS pixels wider on each side (0: 4:3). Set with the runtime's
// VideoSettings::wide_extra (the GPU and Compute renderers).
void set_wide_extra(int extra);

}  // namespace st
