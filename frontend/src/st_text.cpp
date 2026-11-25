#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "st_text.hpp"
#include "st_hooks.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "ntr/recomp.h"

namespace st {
namespace {

// Spirit Tracks (BKIE). The text objects are UnkSystem2_UnkSubSystem11_Derived1 (zeldaret/st, overlay 0): the
// NITRO-System CharCanvas at +4, the font at +32, the 2D engine at +44 (u16, 0 = A), the OBJ entries from +48 (8
// bytes each: the attr0/attr1 word, then attr2 with the palette in bits 12..15), the number of OBJs at +208.
// Phantom Hourglass's (UnkStruct_0203dae0) keeps its canvas at +0x10: here canvases are tracked by their own address.
constexpr uint32_t kTextCanvas = 4, kTextEngine = 44, kTextObj0 = 48, kTextObjAttr2 = 52, kTextObjCount = 208;
constexpr uint32_t kDispCntA = 0x04000000u;
constexpr uint32_t kDispCntB = 0x04001000u;
constexpr uint32_t kMasterBrightA = 0x0400006Cu;
constexpr uint32_t kMasterBrightB = 0x0400106Cu;
constexpr uint32_t kPowCnt1 = 0x04000304u;
constexpr uint32_t kObjPalA = 0x05000200u;
constexpr uint32_t kObjPalB = 0x05000600u;

struct RecordedGlyph {
    uint16_t codepoint = 0;
    int16_t x = 0;          // local canvas X (after NFTR left bearing)
    int16_t y = 0;          // local canvas Y (cell top)
    uint8_t cell_h = 16;
    uint8_t baseline = 14;
    uint8_t glyph_w = 8;
    int8_t char_w = 8;
    uint8_t color_idx = 1;  // `cl` argument passed to NNS_G2dCharCanvasDrawChar
    uint8_t bpp = 2;
};

struct ObjCanvas {
    std::vector<RecordedGlyph> glyphs;
};

struct PendingEntry {
    HdGlyph glyph;
    uint8_t pal_idx = 15;
    uint8_t solid_color_idx = 3;
    uint8_t prio = 0;  // the text object's OBJ priority (attr2 bits 10..11)
};

// A text object drawn this frame: its box and OBJ priority (st_text_oam_flush: text under a panel).
struct DrawnText {
    float x0, y0, x1, y1;
    uint8_t prio;
};

std::atomic<bool> g_enabled{false};
std::mutex g_mutex;
std::unordered_map<uint32_t, ObjCanvas> g_canvases;
std::vector<PendingEntry> g_pending[2];  // indexed by 2D engine (0 = A, 1 = B)
std::vector<HdGlyph> g_visible[2];       // indexed by screen layer (0 = top, 1 = bottom)
std::vector<DrawnText> g_drawn_pending[2];  // every text object drawn this frame, by 2D engine
std::vector<TextBox> g_box_pending[2];   // the text boxes drawn this frame, by 2D engine
std::vector<TextBox> g_box_visible[2];   // ... as shown, by screen layer
std::atomic<bool> g_dual_3d{false};      // two-screen 3D: engine B's text is not shown (set_dual_screen_3d)

stbtt_fontinfo g_font{};
std::atomic<int> g_font_state{0};  // 0 uninit, 1 ready, -1 failed
std::mutex g_font_init_mutex;

const stbtt_fontinfo* ensure_font() {
    int st = g_font_state.load(std::memory_order_acquire);
    if (st == 1) return &g_font;
    if (st == -1) return nullptr;
    std::lock_guard<std::mutex> lock(g_font_init_mutex);
    st = g_font_state.load(std::memory_order_relaxed);
    if (st == 1) return &g_font;
    if (st == -1) return nullptr;
    const int offset = stbtt_GetFontOffsetForIndex(kHdFontData, 0);
    if (offset >= 0 && stbtt_InitFont(&g_font, kHdFontData, offset)) {
        g_font_state.store(1, std::memory_order_release);
        return &g_font;
    }
    g_font_state.store(-1, std::memory_order_release);
    return nullptr;
}

bool valid_main_ram(uint32_t addr) {
    return addr >= 0x02000000u && addr < 0x02400000u;
}

struct NftrMetrics {
    int8_t left = 0;
    uint8_t glyph_w = 0;
    int8_t char_w = 0;
    uint8_t cell_h = 16;
    uint8_t baseline = 14;
    uint8_t bpp = 2;
    uint8_t flags = 0;
};

bool lookup_nftr_metrics(NtrCpu* c, uint32_t pFont, uint16_t ccode, NftrMetrics& out) {
    if (!valid_main_ram(pFont)) return false;
    const uint32_t finf = ntr_rd32(c, pFont);
    if (!valid_main_ram(finf)) return false;
    const uint32_t cglp = ntr_rd32(c, finf + 8u);
    const uint32_t cwdh = ntr_rd32(c, finf + 12u);
    const uint32_t cmap = ntr_rd32(c, finf + 16u);
    if (!valid_main_ram(cglp)) return false;

    out.cell_h = ntr_rd8(c, cglp + 1u);
    out.baseline = ntr_rd8(c, cglp + 4u);
    out.bpp = ntr_rd8(c, cglp + 6u);
    out.flags = ntr_rd8(c, cglp + 7u);

    // 1. Resolve glyph index via CMAP chain (matches NNS_G2dFontFindGlyphIndex at 0x02023EA4).
    uint16_t glyph_idx = 0xFFFFu;
    uint32_t p = cmap;
    for (int iter = 0; iter < 64 && valid_main_ram(p); ++iter) {
        const uint16_t c0 = ntr_rd16(c, p + 0u);
        const uint16_t c1 = ntr_rd16(c, p + 2u);
        if (ccode >= c0 && ccode <= c1) {
            const uint16_t map_type = ntr_rd16(c, p + 4u);
            if (map_type == 0) {
                glyph_idx = uint16_t(ntr_rd16(c, p + 12u) + (ccode - c0));
                break;
            } else if (map_type == 1) {
                glyph_idx = ntr_rd16(c, p + 12u + 2u * uint32_t(ccode - c0));
                break;
            } else if (map_type == 2) {
                const uint16_t count = ntr_rd16(c, p + 12u);
                int lo = 0, hi = int(count) - 1;
                while (lo <= hi) {
                    const int mid = (lo + hi) >> 1;
                    const uint32_t entry = p + 14u + 4u * uint32_t(mid);
                    const uint16_t code = ntr_rd16(c, entry);
                    if (code == ccode) {
                        glyph_idx = ntr_rd16(c, entry + 2u);
                        break;
                    } else if (code < ccode) {
                        lo = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                break;
            }
        }
        p = ntr_rd32(c, p + 8u);
    }
    if (glyph_idx == 0xFFFFu) glyph_idx = ntr_rd16(c, finf + 2u);  // alterCharIndex

    // 2. Resolve widths via CWDH chain (matches NNS_G2dFontGetCharWidthsFromIndex at 0x02023EEC).
    bool found_w = false;
    p = cwdh;
    for (int iter = 0; iter < 64 && valid_main_ram(p); ++iter) {
        const uint16_t i0 = ntr_rd16(c, p + 0u);
        const uint16_t i1 = ntr_rd16(c, p + 2u);
        if (glyph_idx >= i0 && glyph_idx <= i1) {
            const uint32_t off = p + 8u + 3u * uint32_t(glyph_idx - i0);
            out.left = static_cast<int8_t>(ntr_rd8(c, off + 0u));
            out.glyph_w = ntr_rd8(c, off + 1u);
            out.char_w = static_cast<int8_t>(ntr_rd8(c, off + 2u));
            found_w = true;
            break;
        }
        p = ntr_rd32(c, p + 4u);
    }
    if (!found_w) {
        out.left = static_cast<int8_t>(ntr_rd8(c, finf + 4u));
        out.glyph_w = ntr_rd8(c, finf + 5u);
        out.char_w = static_cast<int8_t>(ntr_rd8(c, finf + 6u));
    }
    return true;
}

uint32_t rgb555_to_argb(uint16_t c15, uint8_t bright_mode, uint8_t bright_factor) {
    int r = int(c15 & 0x1Fu);
    int g = int((c15 >> 5) & 0x1Fu);
    int b = int((c15 >> 10) & 0x1Fu);
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    const int f = std::min(int(bright_factor), 16);
    if (bright_mode == 1 && f > 0) {  // fade to white
        r += ((255 - r) * f) / 16;
        g += ((255 - g) * f) / 16;
        b += ((255 - b) * f) / 16;
    } else if (bright_mode == 2 && f > 0) {  // fade to black
        r -= (r * f) / 16;
        g -= (g * f) / 16;
        b -= (b * f) / 16;
    }
    return 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

bool is_whitespace(uint16_t cp) {
    return cp == 0x0020u || cp == 0x00A0u || cp == 0x3000u;
}

}  // namespace

void set_hd_text_enabled(bool enabled) {
    g_enabled.store(enabled, std::memory_order_relaxed);
}

bool hd_text_enabled() {
    return g_enabled.load(std::memory_order_relaxed);
}

const stbtt_fontinfo* hd_font_info() {
    return ensure_font();
}

void hd_text_reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_canvases.clear();
    g_pending[0].clear();
    g_pending[1].clear();
    g_visible[0].clear();
    g_visible[1].clear();
}

namespace {
constexpr uint32_t kHdStateMagic = 0x31544853u;  // "SHT1"
}

std::vector<uint8_t> hd_text_save() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<uint8_t> out;
    auto put = [&](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    };
    const uint32_t head[3] = {kHdStateMagic, uint32_t(sizeof(RecordedGlyph)), uint32_t(g_canvases.size())};
    put(head, sizeof head);
    for (const auto& [addr, canvas] : g_canvases) {
        const uint32_t h2[2] = {addr, uint32_t(canvas.glyphs.size())};
        put(h2, sizeof h2);
        if (!canvas.glyphs.empty()) put(canvas.glyphs.data(), canvas.glyphs.size() * sizeof(RecordedGlyph));
    }
    return out;
}

bool hd_text_restore(const std::vector<uint8_t>& data) {
    size_t at = 0;
    auto get = [&](void* p, size_t n) {
        if (at + n > data.size()) return false;
        std::memcpy(p, data.data() + at, n);
        at += n;
        return true;
    };
    uint32_t head[3];
    if (!get(head, sizeof head) || head[0] != kHdStateMagic || head[1] != sizeof(RecordedGlyph)) return false;
    std::unordered_map<uint32_t, ObjCanvas> canvases;
    for (uint32_t i = 0; i < head[2]; i++) {
        uint32_t h2[2];
        if (!get(h2, sizeof h2) || h2[1] > 4096) return false;
        ObjCanvas& c = canvases[h2[0]];
        c.glyphs.resize(h2[1]);
        if (h2[1] && !get(c.glyphs.data(), h2[1] * sizeof(RecordedGlyph))) return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_canvases = std::move(canvases);
    g_pending[0].clear();
    g_pending[1].clear();
    return true;
}

void text_boxes(int layer, std::vector<TextBox>& out) {
    out.clear();
    if (layer < 0 || layer > 1) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    out = g_box_visible[layer];
}

void set_dual_screen_3d(bool on) { g_dual_3d.store(on, std::memory_order_relaxed); }

void hd_text_draw_list(int layer, std::vector<HdGlyph>& out) {
    out.clear();
    if (layer < 0 || layer > 1) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    out = g_visible[layer];
}

}  // namespace st

// Main 0x02008B58 (before): NNS_G2dCharCanvasInitForOBJ1D(pCC, ...). In Spirit Tracks only the OBJ text objects
// (UnkSystem2_UnkSubSystem11_Derived1, overlay 0) make OBJ canvases: track each one (NPC dialogue, signs, prompts,
// menu labels) and leave BG canvases alone.
extern "C" void st_text_obj_init(NtrCpu* c) {
    std::lock_guard<std::mutex> lock(st::g_mutex);
    st::g_canvases[c->r[0]].glyphs.clear();
}

// Overlay 0 0x02062124 (before): the text object's teardown (releases its OBJ VRAM): r0 = the object.
extern "C" void st_text_obj_deinit(NtrCpu* c) {
    std::lock_guard<std::mutex> lock(st::g_mutex);
    st::g_canvases.erase(c->r[0] + st::kTextCanvas);
}

// Main 0x02008590 (before): ClearContinuous(pCC, cl), the OBJ canvas's whole-canvas clear.
extern "C" void st_text_clear(NtrCpu* c) {
    std::lock_guard<std::mutex> lock(st::g_mutex);
    if (auto it = st::g_canvases.find(c->r[0]); it != st::g_canvases.end()) {
        it->second.glyphs.clear();
    }
}

// Overlay 0 0x02061E98 (before): UnkSystem2_UnkSubSystem11_Base::vfunc_0C(this, cl): fills the whole canvas with one
// colour by MI_CpuFill (not through the SDK's clears): how Spirit Tracks empties a box for its next page.
extern "C" void st_text_obj_clear(NtrCpu* c) {
    std::lock_guard<std::mutex> lock(st::g_mutex);
    if (auto it = st::g_canvases.find(c->r[0] + st::kTextCanvas); it != st::g_canvases.end()) {
        it->second.glyphs.clear();
    }
}

// Main 0x020087F8 (before): ClearArea1D(pCC, cl, x, y, w, h), the OBJ canvas's area clear.
// Removes recorded glyphs inside the cleared rectangle so partial box clears never leave stale text.
extern "C" void st_text_clear_area(NtrCpu* c) {
    const uint32_t this_obj = c->r[0];
    const int32_t x = static_cast<int32_t>(c->r[2]);
    const int32_t y = static_cast<int32_t>(c->r[3]);
    const int32_t w = static_cast<int32_t>(ntr_rd32(c, c->r[13] + 0u));
    const int32_t h = static_cast<int32_t>(ntr_rd32(c, c->r[13] + 4u));
    if (w <= 0 || h <= 0) return;
    std::lock_guard<std::mutex> lock(st::g_mutex);
    auto it = st::g_canvases.find(this_obj);
    if (it == st::g_canvases.end() || it->second.glyphs.empty()) return;
    auto& glyphs = it->second.glyphs;
    glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(), [&](const st::RecordedGlyph& g) {
        const int32_t gx = g.x + std::max<int32_t>(1, g.glyph_w) / 2;
        const int32_t gy = g.y + std::max<int32_t>(1, g.cell_h) / 2;
        return gx >= x && gx < x + w && gy >= y && gy < y + h;
    }), glyphs.end());
}

// Main 0x02008A0C (instead): NNS_G2dCharCanvasDrawChar(pCC, pFont, x, y, cl, ccode).
// When HD text is enabled and the target is a tracked OBJ canvas with a glyph supported by our TrueType font,
// records the glyph and skips 1x bitmap rasterization (returning the exact DS charWidth so line wrap and timing
// remain bit-identical to the original game).
extern "C" int st_text_draw_char(NtrCpu* c) {
    if (!st::g_enabled.load(std::memory_order_relaxed)) return 0;

    const uint32_t pCC = c->r[0];
    const uint32_t pFont = c->r[1];
    const int32_t x = static_cast<int32_t>(c->r[2]);
    const int32_t y = static_cast<int32_t>(c->r[3]);
    const uint32_t cl = ntr_rd32(c, c->r[13] + 0u);
    const uint16_t ccode = ntr_rd16(c, c->r[13] + 4u);
    const uint32_t this_obj = pCC;

    // Never intercept Private Use Area symbols (custom Nintendo/Zelda button & icon glyphs).
    if (ccode >= 0xE000u && ccode <= 0xF8FFu) return 0;

    const stbtt_fontinfo* font = st::ensure_font();
    if (!font) return 0;

    const bool space = st::is_whitespace(ccode);
    if (!space && stbtt_FindGlyphIndex(font, int(ccode)) == 0) return 0;

    st::NftrMetrics m;
    if (!st::lookup_nftr_metrics(c, pFont, ccode, m)) return 0;
    if (m.flags != 0) return 0;  // keep rotated/vertical glyphs on the DS rasterizer

    {
        std::lock_guard<std::mutex> lock(st::g_mutex);
        auto it = st::g_canvases.find(this_obj);
        if (it == st::g_canvases.end()) return 0;  // BG canvas or untracked canvas
        if (!space) {
            st::RecordedGlyph g;
            g.codepoint = ccode;
            g.x = static_cast<int16_t>(x + int32_t(m.left));
            g.y = static_cast<int16_t>(y);
            g.cell_h = m.cell_h > 0 ? m.cell_h : 16;
            g.baseline = m.baseline > 0 ? m.baseline : 14;
            g.glyph_w = m.glyph_w > 0 ? m.glyph_w : static_cast<uint8_t>(std::max<int>(1, m.char_w));
            g.char_w = m.char_w;
            g.color_idx = static_cast<uint8_t>(cl & 0xFu);
            g.bpp = m.bpp > 0 ? m.bpp : 2;
            it->second.glyphs.push_back(g);
        }
    }

    c->r[0] = static_cast<uint32_t>(static_cast<int32_t>(m.char_w));
    return 1;
}

// Overlay 0 0x0206214C and 0x02062208 (before): the text object's two Draws(this, const Vec2s* pos, ...), the second
// one skipping OBJs off the screen (the file select's labels use it), every frame its box is shown.
// It places each of its OBJs at pos plus the OBJ's own offset in the canvas (attr0 Y, attr1 X): the canvas's top
// left is pos plus OBJ 0's offset.
extern "C" void st_text_obj_draw(NtrCpu* c) {
    const uint32_t obj = c->r[0];
    if (static_cast<int32_t>(ntr_rd32(c, obj + st::kTextObjCount)) <= 0) return;  // (as Draw: no OBJs, nothing drawn)
    const uint32_t this_obj = obj + st::kTextCanvas;
    const uint32_t pos = c->r[1];
    const uint32_t w0 = ntr_rd32(c, obj + st::kTextObj0);
    int32_t ox = int32_t((w0 >> 16) & 0x1FFu), oy = int32_t(w0 & 0xFFu);
    if (ox >= 256) ox -= 512;
    if (oy >= 192) oy -= 256;
    const int32_t screen_x = int32_t(int16_t(ntr_rd16(c, pos))) + ox;
    const int32_t screen_y = int32_t(int16_t(ntr_rd16(c, pos + 2u))) + oy;
    // The canvas's size in characters (NNSG2dCharCanvas: areaWidth, areaHeight after charBase).
    const int32_t box_w = int32_t(ntr_rd32(c, this_obj + 4u)) * 8, box_h = int32_t(ntr_rd32(c, this_obj + 8u)) * 8;
    const int engine = ntr_rd16(c, obj + st::kTextEngine) != 0 ? 1 : 0;
    // The 4-bit OBJ palette from OBJ 0's attr2 (bits 12..15).
    const uint8_t pal_idx = static_cast<uint8_t>((ntr_rd16(c, obj + st::kTextObjAttr2) >> 12) & 0xFu);
    const uint8_t prio = static_cast<uint8_t>((ntr_rd16(c, obj + st::kTextObjAttr2) >> 10) & 3u);
    {
        // A text object made before a save state was loaded (Continue) was never seen by st_text_obj_init: track its
        // canvas from here, so its next page of text (after the game clears it) is drawn in HD.
        std::lock_guard<std::mutex> lock(st::g_mutex);
        st::g_canvases.try_emplace(this_obj);
        // Where the box is, for the minimap (with HD text off too).
        if (box_w > 0 && box_h > 0 && box_w <= 512 && box_h <= 384)
            st::g_box_pending[engine].push_back({float(screen_x), float(screen_y), float(screen_x + box_w), float(screen_y + box_h)});
        if (box_w > 0 && box_h > 0 && box_w <= 512 && box_h <= 384)
            st::g_drawn_pending[engine].push_back({float(screen_x), float(screen_y), float(screen_x + box_w), float(screen_y + box_h), prio});
    }

    std::lock_guard<std::mutex> lock(st::g_mutex);
    auto it = st::g_canvases.find(this_obj);
    if (it == st::g_canvases.end() || it->second.glyphs.empty()) return;

    auto& dst = st::g_pending[engine];
    dst.reserve(dst.size() + it->second.glyphs.size());
    for (const st::RecordedGlyph& g : it->second.glyphs) {
        st::PendingEntry e;
        e.glyph.codepoint = g.codepoint;
        e.glyph.ds_x = float(screen_x + int32_t(g.x));
        e.glyph.ds_y = float(screen_y + int32_t(g.y));
        e.glyph.cell_h = float(g.cell_h);
        e.glyph.baseline = float(g.baseline);
        e.glyph.slot_w = float(g.glyph_w);
        e.glyph.char_w = float(g.char_w);
        e.glyph.box_x0 = float(screen_x);
        e.glyph.box_y0 = float(screen_y);
        e.glyph.box_x1 = float(screen_x + std::clamp(box_w, 0, 512));
        e.glyph.box_y1 = float(screen_y + std::clamp(box_h, 0, 384));
        e.pal_idx = pal_idx;
        e.prio = prio;
        // 2bpp NFTR: v=1..3 maps to palette cl + v - 1; solid fill (v=3) is (cl + 2) & 0xF.
        e.solid_color_idx = static_cast<uint8_t>((g.bpp >= 2 ? (g.color_idx + 2u) : g.color_idx) & 0xFu);
        dst.push_back(e);
    }
}

// Main 0x02009B18 (before): func_02009b18(oam_mgr), Phantom Hourglass's NNS_G2dApplyOamManagerToProxy (matched by
// its code), called with each engine's OAM manager after the frame's OBJs are placed.
// Commits each 2D engine's pending HD text list at exact OAM flush time and resolves live OBJ palette & brightness.
extern "C" void st_text_oam_flush(NtrCpu* c) {
    int engine = -1;
    if (c->r[0] == st::addresses().oam_mgr_a) engine = 0;  // (the two OAM managers, A then B: st_hooks.cpp)
    else if (c->r[0] == st::addresses().oam_mgr_b) engine = 1;
    else return;

    const uint32_t dispcnt = ntr_rd32(c, engine == 0 ? st::kDispCntA : st::kDispCntB);
    const bool obj_on = (dispcnt & (1u << 12)) != 0;
    const uint16_t mb = ntr_rd16(c, engine == 0 ? st::kMasterBrightA : st::kMasterBrightB);
    const uint8_t bright_mode = static_cast<uint8_t>((mb >> 14) & 3u);
    const uint8_t bright_factor = static_cast<uint8_t>(mb & 0x1Fu);
    const uint32_t pal_ram = (engine == 0) ? st::kObjPalA : st::kObjPalB;

    // POWCNT1 bit 15: 1 => Engine A on top screen (layer 0); 0 => Engine A on bottom screen (layer 1).
    const bool swap = ((ntr_rd32(c, st::kPowCnt1) >> 15) & 1u) != 0;
    const int layer = (engine == 0) ? (swap ? 0 : 1) : (swap ? 1 : 0);

    std::lock_guard<std::mutex> lock(st::g_mutex);
    auto& pending = st::g_pending[engine];
    // Two-screen 3D (set_dual_screen_3d): a screen keeps the text engine A drew on it last frame, as the picture shown
    // there is engine A's from last frame; engine B's (a copy of that frame) goes nowhere.
    static std::vector<st::HdGlyph> unshown_glyphs;
    static std::vector<st::TextBox> unshown_boxes;
    const bool unshown = engine == 1 && st::g_dual_3d.load(std::memory_order_relaxed);
    auto& visible = unshown ? unshown_glyphs : st::g_visible[layer];
    auto& boxes = unshown ? unshown_boxes : st::g_box_visible[layer];
    visible.clear();
    boxes.clear();
    if (obj_on && !(bright_mode != 0 && bright_factor >= 16)) boxes = st::g_box_pending[engine];
    st::g_box_pending[engine].clear();

    if (obj_on && !pending.empty() && !(bright_mode != 0 && bright_factor >= 16)) {
        visible.reserve(pending.size());
        // Text under a panel: on the DS a background layer drawn between two text objects hides the one behind where
        // they overlap (the collection's letter, its paper a background in front of the letter list's titles). The HD
        // text is drawn over everything, so such text is left out: a glyph of a text object, with another text object
        // of a lower OBJ priority over it and an enabled background whose priority lies between the two.
        const uint32_t bg01 = ntr_rd32(c, engine == 0 ? 0x04000008u : 0x04001008u);
        const uint32_t bg23 = ntr_rd32(c, engine == 0 ? 0x0400000Cu : 0x0400100Cu);
        const uint8_t bg_prio[4] = {uint8_t(bg01 & 3u), uint8_t((bg01 >> 16) & 3u), uint8_t(bg23 & 3u), uint8_t((bg23 >> 16) & 3u)};
        const auto covered = [&](const st::PendingEntry& e) {
            const float gx0 = e.glyph.ds_x, gy0 = e.glyph.ds_y;
            const float gx1 = gx0 + std::max(1.0f, e.glyph.slot_w), gy1 = gy0 + std::max(1.0f, e.glyph.cell_h);
            for (const st::DrawnText& d : st::g_drawn_pending[engine]) {
                if (d.prio >= e.prio || gx1 <= d.x0 || gx0 >= d.x1 || gy1 <= d.y0 || gy0 >= d.y1) continue;
                for (int k = 0; k < 4; k++)
                    if ((dispcnt & (1u << (8 + k))) && bg_prio[k] >= d.prio && bg_prio[k] < e.prio) return true;
            }
            return false;
        };
        for (const st::PendingEntry& e : pending) {
            if (covered(e)) continue;
            st::HdGlyph g = e.glyph;
            const uint32_t addr = pal_ram + uint32_t(e.pal_idx) * 32u + uint32_t(e.solid_color_idx) * 2u;
            const uint16_t c15 = ntr_rd16(c, addr);
            g.argb = st::rgb555_to_argb(c15, bright_mode, bright_factor);
            visible.push_back(g);
        }
    }
    pending.clear();
    st::g_drawn_pending[engine].clear();
}
