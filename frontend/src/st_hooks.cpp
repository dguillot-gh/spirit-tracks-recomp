// Spirit Tracks's hooks (frontend/config/<code>/hooks.txt): the NitroSDK routines the runtime replaces, and DS Protect.
#include "st_hooks.hpp"
#include "st_text.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ntr/nitro.h"
#include "ntr/recomp.h"
#include "st_stick.hpp"

namespace {
// BKIE (USA). The decomp (zeldaret/st) leaves the NitroSDK ones unnamed (OS_func_00nn); each was matched to Phantom
// Hourglass's (same SDK): the same sizes in the same order, and the same literals (ST-PORT.md, "Addresses").
// BKIP (Europe, Revision 0 and 1): the same symbols in the decomp's config/eur and config/eur1 (main's bss sits 0x20
// higher; overlay 0 differs between the two revisions). Mapped by name, not tested on a European ROM yet.
using st::Addresses;
constexpr Addresses kUsa = {0x027E0198, 0x01FFBDC8, 0x0204AEFC, 0x0204C38C, 0x0204AEDC, 0x02078678, 0x020764A0};
constexpr Addresses kEur = {0x027E0198, 0x01FFBDC8, 0x0204AF1C, 0x0204C3AC, 0x0204AEFC, 0x02078698, 0x020764C0};
constexpr Addresses kEur1 = {0x027E0198, 0x01FFBDC8, 0x0204AF1C, 0x0204C3AC, 0x0204AEFC, 0x020786CC, 0x020764F4};
const Addresses* A = &kUsa;
}  // namespace

// Main 0x02022BCC: G3i_PerspectiveW_ (r0, r1 = sin and cos of half the field of view, r2 = the aspect ratio, 4.12).
// The game always passes the DS's 4:3 (0x1555): widescreen gives the wide one instead, and tells the geometry engine
// (third_party/CMakeLists.txt patch) that this scene is wide. A scene of orthographic geometry only (ffcc_wide_block)
// stays 4:3.
extern "C" int ffcc_wide_scene;
extern "C" int ffcc_wide_block;
namespace {
std::atomic<uint32_t> g_aspect{0x1555};
}
extern "C" void st_perspective_aspect(NtrCpu* c) {
    const uint32_t aspect = g_aspect.load(std::memory_order_relaxed);
    if (c->r[2] == 0x1555 && aspect != 0x1555 && ffcc_wide_block == 0) {
        c->r[2] = aspect;
        ffcc_wide_scene = 8;
    }
}

// Main 0x020249EC: G3_LoadMtx44 (r0 = the matrix), `before`. Unlike Phantom Hourglass, Spirit Tracks builds its camera's
// projection (MTX_PerspectiveW, above) only when the field of view changes and then loads the stored matrix every
// frame. So the hook above alone widened a scene for 8 frames only (ffcc_wide_scene counts down), never a scene
// continued from a save state or a widescreen change (its matrix was built at 4:3), and never went back to 4:3. Here a
// perspective matrix (_33 = 0, _23 < 0, nothing off the diagonal in the first two rows) whose aspect (_11 / _00) is one
// of ours (4:3 to 21:9) gets the aspect chosen now, in the game's own copy (so it holds until the game rebuilds it),
// and a wide one keeps the scene marked wide.
extern "C" void st_load_mtx44(NtrCpu* c) {
    const uint32_t m = c->r[0];
    if (ntr_rd32(c, m + 0x3C) != 0) return;                                  // _33: not a perspective projection
    if (int32_t(ntr_rd32(c, m + 0x2C)) >= 0) return;                         // _23
    const int32_t m00 = int32_t(ntr_rd32(c, m + 0x00)), m11 = int32_t(ntr_rd32(c, m + 0x14));
    if (m00 <= 0 || m11 <= 0) return;
    if (ntr_rd32(c, m + 0x04) | ntr_rd32(c, m + 0x08) | ntr_rd32(c, m + 0x10) | ntr_rd32(c, m + 0x18)) return;
    const int64_t now = int64_t(m11) * 4096 / m00;                           // the matrix's aspect, 4.12
    if (now < 0x1500 || now > 0x2800) return;                                // (not one of ours: leave it)
    const uint32_t want = ffcc_wide_block == 0 ? g_aspect.load(std::memory_order_relaxed) : 0x1555u;
    if (std::llabs(now - int64_t(want)) > 8) ntr_wr32(c, m + 0x00, uint32_t(int64_t(m11) * 4096 / int64_t(want)));
    if (want != 0x1555u) ffcc_wide_scene = 8;
}

// Overlay 0 0x020764A0 (func_ov000_020764c0), `before`: the camera builds its view planes, which decide which actors
// are drawn, from its field of view and the DS's shape: one pair of planes is the other scaled by 3/4 (192 / 256), a
// value the function computes once into a table, with its square (two "done" bits guard them). In widescreen NPCs,
// grass and signs then popped in and out at the sides. Here the table holds the picture's own 1 / aspect (and its
// square) and the "done" bits are set, so the planes take in the whole wide picture; back in 4:3, the DS's 3/4. The
// table and the bits are found in the function's literal pool (+0x1C8, +0x1CC; the same function in every release).
extern "C" void st_camera_planes(NtrCpu* c) {
    const uint32_t fn = A->camera_planes;
    const uint32_t guards = ntr_rd32(c, fn + 0x1C8), table = ntr_rd32(c, fn + 0x1CC);
    if (guards - 0x02000000u >= 0x400000u || table - 0x02000000u >= 0x400000u) return;  // (not the expected code)
    const uint32_t aspect = ffcc_wide_block == 0 ? g_aspect.load(std::memory_order_relaxed) : 0x1555u;
    const uint32_t inv = uint32_t((4096u * 4096u + aspect / 2) / aspect);  // 4.12: 0x0C00 at 4:3
    if (ntr_rd32(c, table + 4) != inv) {
        ntr_wr32(c, table + 4, inv);
        ntr_wr32(c, table + 0, (inv * inv + 0x800u) >> 12);
    }
    ntr_wr32(c, guards + 12, ntr_rd32(c, guards + 12) | 1u);
    ntr_wr32(c, guards + 8, ntr_rd32(c, guards + 8) | 1u);
}

// ITCM 0x01FFBD70: OS_IrqHandler.
extern "C" void st_os_irq_handler(NtrCpu* c) { ntr::nitro::irq_handler(c, A->os_irq_table, A->os_irq_handler_thread_switch); }

// DS Protect (overlay 1): its checks are encrypted in the ROM and decrypted only while they run, so their recompiled
// code would be noise. The game calls them through the DSProt_* wrappers, which these replace with the answers of a
// genuine cartridge. DSProt_Detect<X>(callback): nothing detected, returns 0 and never calls the callback.
extern "C" void st_dsprot_detect(NtrCpu* c) { c->r[0] = 0; }
// DSProt_DetectNot<X>(callback): "not X" holds, so the callback (if any) is called and the result is 1.
extern "C" void st_dsprot_detect_not(NtrCpu* c) {
    const uint32_t callback = c->r[0];
    if (callback) ntr_call(c, callback);
    c->r[0] = 1;
}

namespace st {

const Addresses& addresses() { return *A; }

bool select_release(const std::string& game_code, int revision) {
    if (game_code == "BKIE") A = &kUsa;
    else if (game_code == "BKIP" && revision == 0) A = &kEur;
    else if (game_code == "BKIP" && revision == 1) A = &kEur1;
    else return false;
    return true;
}

void* find_hook(const char* name) {
    static const struct {
        const char* name;
        void* fn;
    } kHooks[] = {
        {"st_os_irq_handler", reinterpret_cast<void*>(&st_os_irq_handler)},
        {"st_perspective_aspect", reinterpret_cast<void*>(&st_perspective_aspect)},
        {"st_load_mtx44", reinterpret_cast<void*>(&st_load_mtx44)},
        {"st_camera_planes", reinterpret_cast<void*>(&st_camera_planes)},
        {"st_dsprot_detect", reinterpret_cast<void*>(&st_dsprot_detect)},
        {"st_dsprot_detect_not", reinterpret_cast<void*>(&st_dsprot_detect_not)},
        {"st_stick_move", reinterpret_cast<void*>(&st_stick_move)},
        {"ntr_nitro_save_context", reinterpret_cast<void*>(&ntr_nitro_save_context)},
        {"ntr_nitro_load_context", reinterpret_cast<void*>(&ntr_nitro_load_context)},
        {"ntr_nitro_init_context", reinterpret_cast<void*>(&ntr_nitro_init_context)},
        {"st_text_obj_init", reinterpret_cast<void*>(&st_text_obj_init)},
        {"st_text_obj_deinit", reinterpret_cast<void*>(&st_text_obj_deinit)},
        {"st_text_clear", reinterpret_cast<void*>(&st_text_clear)},
        {"st_text_obj_clear", reinterpret_cast<void*>(&st_text_obj_clear)},
        {"st_text_clear_area", reinterpret_cast<void*>(&st_text_clear_area)},
        {"st_text_draw_char", reinterpret_cast<void*>(&st_text_draw_char)},
        {"st_text_obj_draw", reinterpret_cast<void*>(&st_text_obj_draw)},
        {"st_text_oam_flush", reinterpret_cast<void*>(&st_text_oam_flush)},
    };
    for (const auto& h : kHooks)
        if (std::strcmp(h.name, name) == 0) return h.fn;
    return nullptr;
}

void set_wide_extra(int extra) {
    g_aspect = extra > 0 ? uint32_t(((256 + 2 * extra) * 4096 + 96) / 192) : 0x1555u;
}

}  // namespace st
