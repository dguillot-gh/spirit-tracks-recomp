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
    // The native D-pad controls (st_pad.cpp): the return addresses of the calls they act at, and functions they call.
    uint32_t pad_move_ret;                  // func_ov017_020be44c's call of func_ov000_02096af4 (Link stops)
    uint32_t pad_action_ret;                // ov000 0x020888F0's call of 0x02088834 (a slash under way)
    uint32_t pad_by_ret;                    // func_ov017_020bcde8's call of func_ov102_0218323c
    uint32_t pad_by_fn;                     // func_ov102_0218323c
    uint32_t pad_a_hold_fn;                 // ov000 0x02070128 (stylus held, for an object)
    uint32_t pad_world_to_screen;           // UnkStruct_027e09bc_0C::func_01ffd43c (ITCM)
    uint32_t pad_touch_update;              // TouchControl::UpdateFlags
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
