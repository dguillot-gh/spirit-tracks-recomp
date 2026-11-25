#include <array>
#include <cstdio>

#include "ntr/rom.hpp"

namespace ntr {

uint32_t crc32(std::span<const uint8_t> data) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xFFFFFFFFu;
    for (uint8_t b : data) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return ~c;
}

std::string crc32_hex(uint32_t crc) {
    char buf[9];
    std::snprintf(buf, sizeof buf, "%08x", crc);
    return buf;
}

// Spirit Tracks (USA). The clean dump is the one the zeldaret/st decomp's config (frontend/config/bkie) describes. The
// D-pad and D-pad + QoL patches (d0k3, Greiga Master / StraDaMa, v0.9c) are made from it by the build scripts
// (tools/bps_patch.py). All three share one config and one recompiled game: the patches move no function starts. Their
// ARM9 code differs only by the D-pad hack (12 new functions at 0x02044b00 and 7 changed ones), which a module built
// from a patched ROM carries; the QoL hack is in the ARM7, which comes from the ROM, and the files are the same by path.
// So a build plays any of them (main.cpp): the clean dump then gets the D-pad controls without the QoL changes.
static const KnownRom kKnownRoms[] = {
    {"1f0921b5e99db9d92d24710f1b004e4f3c997b73", "38392357", 134217728, "BKIE", "USA",
     "The Legend of Zelda - Spirit Tracks (USA, Australia) (En,Fr,Es)", true},
    {"6a63d88702c34da626e273dfca4931d52ac4551b", "a966458d", 94096060, "BKIE", "USA (D-pad patch)",
     "The Legend of Zelda - Spirit Tracks (USA) (D-pad patch v0.9c)", true},
    {"6896c1db2b3a725cb991f711a875fc2228b7620e", "44c7c45b", 94097260, "BKIE", "USA (D-pad + QoL patch)",
     "The Legend of Zelda - Spirit Tracks (USA) (D-pad + QoL patch v0.9c)", true},
    // Europe (BKIP, En,Fr,De,Es,It), Revision 0 and Revision 1: their overlays differ, so each has its own config
    // (frontend/config/bkip, bkip1, from the zeldaret/st decomp's config/eur and config/eur1) and its own build. The
    // SHA-1s are the decomp's (st_eur.sha1, st_eur1.sha1: Revision 1 has several dumps). No D-pad patch for Europe.
    {"9e99cc803a14ce038eb908db585431f8254f09ee", "", 0, "BKIP", "Europe",
     "The Legend of Zelda - Spirit Tracks (Europe) (En,Fr,De,Es,It)", true, 0},
    {"dcb221a89acb894503a461c2c7d4cfdad9a06b68", "", 0, "BKIP", "Europe (Rev 1)",
     "The Legend of Zelda - Spirit Tracks (Europe) (En,Fr,De,Es,It) (Rev 1)", true, 1},
    {"b9096f46e9163750a9e3f6c2fc0758e6094b6135", "", 0, "BKIP", "Europe (Rev 1)",
     "The Legend of Zelda - Spirit Tracks (Europe) (En,Fr,De,Es,It) (Rev 1)", true, 1},
    {"b0288a880498c12508e53f4373085eb88955e14b", "", 0, "BKIP", "Europe (Rev 1)",
     "The Legend of Zelda - Spirit Tracks (Europe) (En,Fr,De,Es,It) (Rev 1)", true, 1},
    {"4efbff9a263fe622f3f7e7cfb7bea8046b86e57d", "", 0, "BKIP", "Europe (Rev 1)",
     "The Legend of Zelda - Spirit Tracks (Europe) (En,Fr,De,Es,It) (Rev 1)", true, 1},
    {"52d7f22255d5d86fc40d3a43c7de969e0365ff7d", "", 0, "BKIP", "Europe (Rev 1)",
     "The Legend of Zelda - Spirit Tracks (Europe) (En,Fr,De,Es,It) (Rev 1)", true, 1},
};

std::span<const KnownRom> known_roms() { return kKnownRoms; }

const KnownRom* find_known_rom(const Sha1Digest& digest) {
    std::string hex = to_hex(digest);
    for (const KnownRom& r : kKnownRoms)
        if (hex == r.sha1_hex) return &r;
    return nullptr;
}

const KnownRom* find_known_rom(std::span<const uint8_t> rom) {
    if (const KnownRom* r = find_known_rom(sha1(rom))) return r;
    const std::string crc = crc32_hex(crc32(rom));
    for (const KnownRom& r : kKnownRoms)
        if (!*r.sha1_hex && crc == r.crc32_hex && rom.size() == r.size) return &r;
    return nullptr;
}

}  // namespace ntr
