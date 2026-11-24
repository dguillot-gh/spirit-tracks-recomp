// The shell APK's game builder: the player's ROM → C (the recompiler, as in ntr-recomp) → libst_game.so (TinyCC),
// all on the phone. Called from the launcher (GameBuilder.java) on a worker thread.
#include <jni.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "libtcc.h"
#include "ntr/rom.hpp"
#include "recomp/recomp.hpp"

namespace fs = std::filesystem;

static std::atomic<int> g_progress{0};  // 0-100
static std::string g_tcc_error;

static void on_tcc_error(void*, const char* msg) {
    if (g_tcc_error.size() < 2000) g_tcc_error += std::string(msg) + "\n";
}

static TCCState* new_tcc(const std::string& tcc_dir, int type) {
    TCCState* s = tcc_new();
    if (!s) return nullptr;
    tcc_set_error_func(s, nullptr, on_tcc_error);
    tcc_set_lib_path(s, tcc_dir.c_str());
    tcc_set_options(s, "-nostdlib -w");
    tcc_set_output_type(s, type);
    return s;
}

// Returns "" on success, else a message for the player.
static std::string build(const std::string& rom_path, const std::string& assets, const std::string& out_so,
                         const std::string& work) {
    g_progress = 0;
    std::error_code ec;
    const fs::path gen = fs::path(work) / "gen", obj = fs::path(work) / "obj";
    fs::remove_all(work, ec);
    fs::create_directories(gen, ec);
    fs::create_directories(obj, ec);

    // ---- 1. recompile (ntr-recomp's steps) ----
    try {
        ntr::Rom rom = ntr::Rom::open(rom_path);
        const ntr::KnownRom* known = ntr::find_known_rom(rom.bytes());
        if (!known || !known->supported) return "This ROM is not a supported Spirit Tracks dump.";
        recomp::Program program = recomp::load_program(rom);
        program.rom_sha1 = ntr::to_hex(rom.sha1());
        const std::string config = assets + "/config";
        recomp::load_regions(program, config);
        program.config = recomp::load_config(program, config);
        recomp::Analysis analysis = recomp::analyze(program);
        recomp::emit_c(program, analysis, gen.string());
    } catch (const std::exception& e) {
        return std::string("Recompiling failed: ") + e.what();
    }
    g_progress = 20;

    // ---- 2. compile each C file to an object (one TinyCC state each, as tested), then link ----
    std::vector<fs::path> sources;
    for (const auto& e : fs::directory_iterator(gen, ec))
        if (e.path().extension() == ".c") sources.push_back(e.path());
    if (sources.empty()) return "Recompiling wrote no C files.";
    const std::string tcc_dir = assets + "/tcc";
    std::vector<std::string> objects;
    for (size_t i = 0; i < sources.size(); i++) {
        g_tcc_error.clear();
        TCCState* s = new_tcc(tcc_dir, TCC_OUTPUT_OBJ);
        if (!s) return "Out of memory starting the compiler.";
        tcc_add_include_path(s, gen.string().c_str());
        tcc_add_include_path(s, (assets + "/rt").c_str());
        tcc_add_include_path(s, (tcc_dir + "/include").c_str());
        tcc_define_symbol(s, "NTR_GAME_MODULE", "1");
        const std::string o = (obj / sources[i].stem()).string() + ".o";
        const bool ok = tcc_add_file(s, sources[i].string().c_str()) == 0 && tcc_output_file(s, o.c_str()) == 0;
        tcc_delete(s);
        if (!ok) return "Compiling " + sources[i].filename().string() + " failed:\n" + g_tcc_error;
        objects.push_back(o);
        g_progress = 20 + int(75 * (i + 1) / sources.size());
    }
    g_tcc_error.clear();
    TCCState* s = new_tcc(tcc_dir, TCC_OUTPUT_DLL);
    if (!s) return "Out of memory starting the linker.";
    bool ok = true;
    for (const auto& o : objects) ok = ok && tcc_add_file(s, o.c_str()) == 0;
    ok = ok && tcc_add_file(s, "/system/lib64/libc.so") == 0;  // memcpy and friends
    const std::string tmp = out_so + ".tmp";
    fs::create_directories(fs::path(out_so).parent_path(), ec);
    ok = ok && tcc_output_file(s, tmp.c_str()) == 0;
    tcc_delete(s);
    if (!ok) return "Linking the game failed:\n" + g_tcc_error;
    fs::rename(tmp, out_so, ec);
    if (ec) return "Cannot store the game: " + ec.message();
    fs::remove_all(work, ec);  // (the C and objects: ~200 MB)
    g_progress = 100;
    return "";
}

static std::string str(JNIEnv* env, jstring s) {
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string r(c);
    env->ReleaseStringUTFChars(s, c);
    return r;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_strecomp_st_GameBuilder_build(JNIEnv* env, jclass, jstring rom,
                                                                           jstring assets, jstring out, jstring work) {
    const std::string err = build(str(env, rom), str(env, assets), str(env, out), str(env, work));
    return err.empty() ? nullptr : env->NewStringUTF(err.c_str());
}

extern "C" JNIEXPORT jint JNICALL Java_com_strecomp_st_GameBuilder_progress(JNIEnv*, jclass) { return g_progress; }
