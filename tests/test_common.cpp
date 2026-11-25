// Unit tests for ntr_common that need no game data.
#include <cstdio>
#include <cstring>
#include <string>

#include "ntr/rom.hpp"

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                   \
        }                                                                 \
    } while (0)

static std::string sha1_hex(const char* s) {
    return ntr::to_hex(ntr::sha1({reinterpret_cast<const uint8_t*>(s), std::strlen(s)}));
}

static void test_sha1() {
    CHECK(sha1_hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(sha1_hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    std::string million(1000000, 'a');
    CHECK(sha1_hex(million.c_str()) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

static void test_crc32() {
    const auto crc = [](const char* s) {
        return ntr::crc32_hex(ntr::crc32({reinterpret_cast<const uint8_t*>(s), std::strlen(s)}));
    };
    CHECK(crc("") == "00000000");
    CHECK(crc("123456789") == "cbf43926");  // the standard check value
}

static void test_blz() {
    // Region = uncompressed prefix "XY" + compressed stream + footer. The stream is read backwards:
    // flag byte 0x08 = four literals (D, C, B, A) then one reference (len 18, disp 4).
    const uint8_t compressed[] = {
        'X', 'Y',                 // uncompressed prefix (left untouched)
        0x01, 0xF0,               // reference: v = 0xF001 -> len 15+3, disp 1+3
        'A', 'B', 'C', 'D',       // literals, consumed from the end
        0x08,                     // flags
        0xFF,                     // padding covered by the footer length
        0x10, 0x00, 0x00, 0x09,   // compressed length 0x10, footer length 9
        0x06, 0x00, 0x00, 0x00,   // output grows by 6 bytes
    };
    auto out = ntr::blz_decompress(compressed);
    CHECK(std::string(out.begin(), out.end()) == "XYCDABCDABCDABCDABCDABCD");
}

static ntr::Sha1Digest digest_of(const std::string& hex) {
    ntr::Sha1Digest d{};
    for (size_t i = 0; i < d.size(); i++) d[i] = uint8_t(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    return d;
}

static void test_known_roms() {
    const ntr::KnownRom* clean = ntr::find_known_rom(digest_of("1f0921b5e99db9d92d24710f1b004e4f3c997b73"));
    CHECK(clean && std::string(clean->game_code) == "BKIE" && clean->supported);
    if (clean) CHECK(std::string(clean->crc32_hex) == "38392357");
    // The patched variants: by CRC-32 and size while their SHA-1 is not listed.
    int patched = 0;
    for (const ntr::KnownRom& r : ntr::known_roms())
        if (!*r.sha1_hex) {
            patched++;
            CHECK(r.size != 0 && std::strlen(r.crc32_hex) == 8 && std::string(r.game_code) == "BKIE");
        }
    CHECK(patched <= 2);
    // Clean, D-pad and D-pad + QoL are all listed by SHA-1 as BKIE: one build plays all three (main.cpp).
    for (const char* hex : {"1f0921b5e99db9d92d24710f1b004e4f3c997b73", "6a63d88702c34da626e273dfca4931d52ac4551b",
                            "6896c1db2b3a725cb991f711a875fc2228b7620e"}) {
        const ntr::KnownRom* r = ntr::find_known_rom(digest_of(hex));
        CHECK(r && std::string(r->game_code) == "BKIE" && r->supported);
    }
    // Europe: Revision 0 and Revision 1 (several dumps) are BKIP, told apart by the revision (their own configs).
    const ntr::KnownRom* eur = ntr::find_known_rom(digest_of("9e99cc803a14ce038eb908db585431f8254f09ee"));
    CHECK(eur && std::string(eur->game_code) == "BKIP" && eur->supported && eur->revision == 0);
    for (const char* hex : {"dcb221a89acb894503a461c2c7d4cfdad9a06b68", "b9096f46e9163750a9e3f6c2fc0758e6094b6135",
                            "b0288a880498c12508e53f4373085eb88955e14b", "4efbff9a263fe622f3f7e7cfb7bea8046b86e57d",
                            "52d7f22255d5d86fc40d3a43c7de969e0365ff7d"}) {
        const ntr::KnownRom* r = ntr::find_known_rom(digest_of(hex));
        CHECK(r && std::string(r->game_code) == "BKIP" && r->supported && r->revision == 1);
    }
    const std::vector<uint8_t> junk(64, 0);
    CHECK(!ntr::find_known_rom(std::span<const uint8_t>(junk)));
    CHECK(!ntr::find_known_rom(digest_of("0000000000000000000000000000000000000000")));
    // No dump is listed twice.
    for (const ntr::KnownRom& a : ntr::known_roms())
        for (const ntr::KnownRom& b : ntr::known_roms())
            if (&a != &b) CHECK(std::strcmp(a.sha1_hex, b.sha1_hex) != 0 || std::strcmp(a.crc32_hex, b.crc32_hex) != 0);
}

int main() {
    test_known_roms();
    test_sha1();
    test_crc32();
    test_blz();
    if (failures) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::puts("all tests passed");
    return 0;
}
