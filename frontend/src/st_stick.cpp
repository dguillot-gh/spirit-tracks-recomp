#include "st_stick.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

#include "ntr/runtime.hpp"
#include "st_hooks.hpp"

namespace st {
namespace {

std::atomic<int> g_x{0}, g_y{0};  // the stick, -1000..1000 (y down)

// BKIE (USA), the D-pad patch v0.9c (its routines at 0x02044B00..0x02044D78, read from the patched ARM9). The game's
// input pointer (0x0204A0F0 + 3564; held buttons at +288) and the camera's yaw function (r0 = the camera, r8 here) are
// per release: st_hooks.cpp, addresses().
constexpr uint32_t kHeldOffset = 288;
// UnkStruct_027e0ce0_40_Base_14 (r4 here; zeldaret/st include/Unknown/UnkStruct_027e0ce0.hpp):
constexpr uint32_t kDist = 0x3C;      // mUnk_3C: the stylus's distance from Link, fx32 pixels
constexpr uint32_t kUnk40 = 0x40;     // mUnk_40, mUnk_5C: cleared, as the patch does
constexpr uint32_t kUnk5C = 0x5C;
constexpr uint32_t kAngle = 0x44;     // mUnk_44, mUnk_46: the direction
constexpr uint32_t kAngle2 = 0x46;
constexpr uint32_t kSelect = 0x0004, kY = 0x0800;  // DS keys (held)

constexpr float kDeadZone = 0.22f;
// The patch's distances (fx32 pixels): 0x60000 = 96 for Y + D-pad (walk), 0x5F0000 = 1520 for the D-pad (run).
// Measured on the phone (2026-11-22, 2 s pushes from one spot): 77 does not move Link, 224 and more run as far as the
// D-pad. So the stick walks from 96 up to 200 over most of its travel and runs (the patch's 1520) at the rim.
constexpr float kWalkTop = 200.0f, kRun = 1520.0f, kWalkY = 96.0f;
constexpr float kRunFrom = 0.8f;  // (of the travel past the dead zone)

}  // namespace

void set_stick(float x, float y) {
    g_x.store(int(std::clamp(x, -1.0f, 1.0f) * 1000.0f), std::memory_order_relaxed);
    g_y.store(int(std::clamp(y, -1.0f, 1.0f) * 1000.0f), std::memory_order_relaxed);
}

uint32_t stick_dpad(float x, float y) {
    const float mag = std::hypot(x, y);
    if (mag < kDeadZone) return 0;
    constexpr float kSin22 = 0.3827f;  // (8 sectors of 45 degrees)
    uint32_t bits = 0;
    if (x / mag > kSin22) bits |= ntr::KeyRight;
    if (x / mag < -kSin22) bits |= ntr::KeyLeft;
    if (y / mag > kSin22) bits |= ntr::KeyDown;
    if (y / mag < -kSin22) bits |= ntr::KeyUp;
    return bits;
}

}  // namespace st

extern "C" int st_stick_move(NtrCpu* c) {
    const float x = float(st::g_x.load(std::memory_order_relaxed)) / 1000.0f;
    const float y = float(st::g_y.load(std::memory_order_relaxed)) / 1000.0f;
    const float mag = std::min(1.0f, std::hypot(x, y));
    if (mag < st::kDeadZone) return 0;
    const uint32_t input = ntr_rd32(c, st::addresses().input_ptr);
    const uint32_t held = (input >= 0x02000000u && input < 0x02400000u) ? ntr_rd32(c, input + st::kHeldOffset) : 0;
    if (held & st::kSelect) return 0;

    // The camera's yaw, as the patch asks for it (the call may change any register: they are put back after).
    uint32_t saved[16];
    std::memcpy(saved, c->r, sizeof saved);
    c->r[0] = c->r[8];
    ntr_call(c, st::addresses().camera_yaw);
    const uint32_t yaw = c->r[0];
    std::memcpy(c->r, saved, sizeof saved);

    const float t = (mag - st::kDeadZone) / (1.0f - st::kDeadZone);
    float dist = t >= st::kRunFrom ? st::kRun : st::kWalkY + (t / st::kRunFrom) * (st::kWalkTop - st::kWalkY);
    if (held & st::kY) dist = std::min(dist, st::kWalkY);
    const int32_t dir = int32_t(std::lround(std::atan2(x, y) * (65536.0 / 6.283185307179586)));
    const uint32_t angle = (yaw + uint32_t(dir)) & 0xFFFFu;
    const uint32_t obj = c->r[4];
    ntr_wr32(c, obj + st::kDist, uint32_t(std::lround(dist * 4096.0f)));
    ntr_wr16(c, obj + st::kAngle, angle);
    ntr_wr16(c, obj + st::kUnk40, 0);
    ntr_wr16(c, obj + st::kUnk5C, 0);
    ntr_wr16(c, obj + st::kAngle2, angle);
    return 1;
}
