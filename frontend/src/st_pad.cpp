// The D-pad controls, native: what d0k3's D-pad patch (v0.9c, ST-PORT.md) changes in the game's code, done by hooks
// (hooks.txt), so a game built from any release's clean ROM has them, the US one and both European ones.
//
// The patch branches from 10 places in the game's code into 12 routines of its own (0x02044B00..0x02044D78 of its
// ARM9). Each hook below is one of those places and does what the patch's routine does there:
//
//   - the D-pad (or the analog stick) walks and runs Link (Y held: walk) where the game stops him for want of a stylus;
//   - A taps the ground just in front of Link (talk, pick up, attack) when no direction is held, and A with a direction
//     rolls; B swings the sword (with a direction: the other swing); B + Y does one more action (the patch's 0x02044CE0);
//   - holding A counts as the stylus held for the function at 0x02070128 (the patch's 0x02044D44);
//   - the game's own uses of Right, Down and Select on the field (AdventureModeManager) move to Select + Right,
//     Select + Down and Select + Up, and one of A / Left (overlay 93) to Select + Left: the D-pad walks now.
//
// A build made from the patched ROM has the patch's code at these places, so its hooks.txt lines are left out there
// (each names the clean instruction it expects), and the call-site hooks act only on the clean ROM's return address.
#include <cmath>
#include <cstring>

#include "ntr/runtime.hpp"
#include "st_hooks.hpp"
#include "st_stick.hpp"

namespace {

// The game's buttons (DS order, X and Y above): held at the input + 0x120, pressed this frame at + 0x122.
constexpr uint32_t kA = 0x001, kB = 0x002, kSelect = 0x004, kRight = 0x010, kLeft = 0x020, kUp = 0x040, kDown = 0x080,
                   kY = 0x800, kDirs = 0x0F0;

uint32_t input(NtrCpu* c) {
    const uint32_t p = ntr_rd32(c, st::addresses().input_ptr);
    return p - 0x02000000u < 0x400000u ? p : 0;
}
uint32_t held(NtrCpu* c) {
    const uint32_t p = input(c);
    return p ? ntr_rd16(c, p + 0x120) : 0;
}
uint32_t pressed(NtrCpu* c) {
    const uint32_t p = input(c);
    return p ? ntr_rd16(c, p + 0x122) : 0;
}

// The camera's yaw (r0 = the camera); every register is put back after the call.
uint32_t camera_yaw(NtrCpu* c, uint32_t camera) {
    uint32_t saved[16];
    std::memcpy(saved, c->r, sizeof saved);
    c->r[0] = camera;
    ntr_call(c, st::addresses().camera_yaw);
    const uint32_t yaw = c->r[0];
    std::memcpy(c->r, saved, sizeof saved);
    return yaw;
}

// Calls a hooked function's own body from its `instead` hook (the hook sees `inside` set and lets it run).
thread_local bool inside = false;

}  // namespace

// Overlay 0 0x02096AD4 (UnkStruct_027e0ce0_40_Base_14::func_ov000_02096af4: Link stops), `instead`. Called by the
// touch movement (func_ov017_020be44c) when no stylus is down, where the patch calls its 0x02044B28 instead: the stick
// (st_stick_move) or the D-pad moves Link and he does not stop.
extern "C" int st_pad_move(NtrCpu* c) {
    if (c->r[14] != st::addresses().pad_move_ret) return 0;
    if (st_stick_move(c)) return 1;
    const uint32_t h = held(c);
    if (h & kSelect) return 0;
    // The patch's table (0x02044BA4): a direction, and its angle from the camera's (0 is towards it, down the screen).
    static const struct {
        uint32_t keys, angle;
    } kDir[] = {{kUp, 0x8000}, {kDown, 0x0000}, {kLeft, 0xC000}, {kRight, 0x4000},
                {kUp | kRight, 0x6000}, {kDown | kRight, 0x2000}, {kUp | kLeft, 0xA000}, {kDown | kLeft, 0xE000}};
    uint32_t angle = 0x10000;
    for (const auto& d : kDir)
        if ((h & kDirs) == d.keys) angle = d.angle;
    if (angle > 0xFFFF) return 0;
    const uint32_t obj = c->r[4];  // (= r0)
    ntr_wr32(c, obj + 0x3C, (h & kY) ? 0x60000u : 0x5F0000u);  // the stylus's distance: 96 (walk) or 1520 pixels
    const uint32_t a = (camera_yaw(c, c->r[8]) + angle) & 0xFFFFu;
    ntr_wr16(c, obj + 0x44, a);
    ntr_wr16(c, obj + 0x40, 0);
    ntr_wr16(c, obj + 0x5C, 0);
    ntr_wr16(c, obj + 0x46, a);
    return 1;
}

// Overlay 0 0x02088834 (r0 = Link's sword state; 1 when a stylus slash is under way), `instead`. From 0x020888F0
// (which slash: its r4, -1 for none) the patch's 0x02044CB8 also makes B a slash (2) and B with a direction another (3).
extern "C" int st_pad_action(NtrCpu* c) {
    if (c->r[14] != st::addresses().pad_action_ret) return 0;
    const uint32_t s = c->r[0];
    uint32_t r = 0;  // (the function's own test, a leaf: it is done here)
    if (int32_t(ntr_rd32(c, s + 0x6C)) > 2048) {
        const int32_t t = int16_t(ntr_rd16(c, s + 0x70)), u = int16_t(ntr_rd16(c, s + 0x54));
        if (t > 0 && t <= u && u < t + 15) r = 1;
    }
    if (pressed(c) & kB) c->r[4] = (held(c) & kDirs) ? 3 : 2;
    c->r[0] = r;
    return 1;
}

// Overlay 0 0x02070128 (r0 = an object; whether the stylus is held, for it), `instead`: 0 becomes 95 while A is held
// (the patch's 0x02044D44).
extern "C" int st_pad_a_hold(NtrCpu* c) {
    const uint32_t obj = c->r[0];
    uint32_t r = 0;  // (the function's own: its literal at + 0x18 is the address of a byte flag 0x28 on)
    if (ntr_rd8(c, obj + 5) != 0) r = ntr_rd8(c, ntr_rd32(c, st::addresses().pad_a_hold_fn + 0x18) + 0x28);
    if (r == 0 && (held(c) & kA)) r = 95;
    c->r[0] = r;
    return 1;
}

// Overlay 17 0x020BE4E4 in func_ov017_020be44c (the touch movement; r0 = r8 = the camera, r4 = Link's touch control,
// r5 = Link's position), `at`: A pressed, with no direction held, touches the screen 30 pixels in front of Link, as
// the patch's 0x02044BFC does. The function then goes on as for a stylus.
extern "C" void st_pad_a_tap(NtrCpu* c) {
    if (held(c) & kDirs) return;
    const uint32_t p = pressed(c);
    if (!(p & kA)) return;
    const uint32_t camera = c->r[0], touch = c->r[4];
    uint32_t saved[16];
    std::memcpy(saved, c->r, sizeof saved);
    c->r[13] -= 16;  // (a Vec2s on the stack: Link's place on the screen)
    const uint32_t out = c->r[13];
    c->r[0] = camera, c->r[1] = out, c->r[2] = saved[5], c->r[3] = p;
    ntr_call(c, st::addresses().pad_world_to_screen);
    int32_t x = int16_t(ntr_rd16(c, out)), y = int16_t(ntr_rd16(c, out + 2));
    std::memcpy(c->r, saved, sizeof saved);
    // Link's facing from the camera (not wrapped, as the patch compares it), and the patch's 30-pixel offsets
    const int32_t a = int32_t(camera_yaw(c, camera) + ntr_rd16(c, touch + 0x44));
    x += a == 0 ? 0 : a < 0x8000 ? 30 : a > 0x8000 ? -30 : 0;
    y += a <= 0x4000 ? (a < 0x4000 ? 30 : 0) : a > 0xC000 ? 30 : a < 0xC000 ? -30 : 0;
    ntr_wr8(c, touch + 0x0C, 1);
    ntr_wr16(c, touch + 0x0E, uint32_t(x) & 0xFFFFu);
    ntr_wr16(c, touch + 0x10, uint32_t(y) & 0xFFFFu);
    c->r[0] = touch, c->r[1] = 2;
    ntr_call(c, st::addresses().pad_touch_update);  // TouchControl::UpdateFlags(2)
    std::memcpy(c->r, saved, sizeof saved);
    c->r[1] = ntr_rd8(c, touch + 0x0C);  // (the function read it just before: now touched)
}

// Overlay 17 0x020BD324 in func_ov017_020bd300 (r7 = Link's action state, r12 its new flags), `at`: A held with a
// direction adds flag 2 (a roll) and clears + 0x1C, as the patch's 0x02044D10.
extern "C" void st_pad_roll(NtrCpu* c) {
    const uint32_t h = held(c);
    if (!(h & kA) || !(h & kDirs)) return;
    c->r[12] |= 2;
    ntr_wr32(c, c->r[7] + 0x1C, 0);
}

// Overlay 102 0x0218321C (UnkStruct_027e0ce0_40_Base_74::func_ov102_0218323c), `instead`: called from
// func_ov017_020bcde8, after which B + Y held sets its object's + 0x58 to 5 (the patch's 0x02044CE0).
extern "C" int st_pad_by(NtrCpu* c) {
    if (inside || c->r[14] != st::addresses().pad_by_ret) return 0;
    const uint32_t obj = c->r[0];
    inside = true;
    ntr_call(c, st::addresses().pad_by_fn);
    inside = false;
    if ((held(c) & (kB | kY)) == (kB | kY)) ntr_wr32(c, obj + 0x58, 5);
    return 1;
}

// Overlay 24, AdventureModeManager::vfunc_24, `at` its tests of the pressed buttons (r0): Right, Down and Select there
// are Select + Right, Select + Down and Select + Up (held), and the Y and B tests after them never hold, as in the patch.
extern "C" void st_pad_keys_right(NtrCpu* c) { c->r[0] = (held(c) & (kSelect | kRight)) == (kSelect | kRight) ? kRight : 0; }
extern "C" void st_pad_keys_down(NtrCpu* c) { c->r[0] = (held(c) & (kSelect | kDown)) == (kSelect | kDown) ? kDown : 0; }
extern "C" void st_pad_keys_select(NtrCpu* c) { c->r[0] = (held(c) & (kSelect | kUp)) == (kSelect | kUp) ? kSelect : 0; }
// Overlay 93 0x02176010 (r10 + 8 -> held and pressed buttons), `at` its test for A or Left pressed: Select held with
// Left pressed.
extern "C" void st_pad_keys_left(NtrCpu* c) {
    const uint32_t keys = ntr_rd32(c, ntr_rd32(c, c->r[10] + 8));
    c->r[0] = (keys & 0x200004u) == 0x200004u ? kLeft : 0;
}
