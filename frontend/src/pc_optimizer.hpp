// The optimised game build on Windows: after TinyCC's quick build (game_builder.cpp), the same C is compiled again
// with clang -O2 in the background, at low priority, several files at a time, and linked with lld into
// game/st_game.opt.dll, which the next start loads instead (a loaded DLL can't be replaced). clang, lld and their DLLs
// travel inside the exe (tools/embed_clang.cmake, -DST_PC_CLANG_DIR) and are unpacked only while building.
#pragma once

#include <atomic>
#include <filesystem>
#include <string>

namespace st {

struct ClangPackEntry {
    const char* name;
    unsigned long long offset, size;
};
extern const ClangPackEntry kClangPack[];
extern const int kClangPackCount;

// True when this exe carries clang.
bool pc_opt_available();

// Where the quick build leaves the C and its headers for the optimised build (data/opt).
std::filesystem::path pc_opt_dir(const std::filesystem::path& data_dir);

// The optimised module and its stamp (the SHA-1 of the exe that built it).
std::filesystem::path pc_opt_module(const std::filesystem::path& data_dir);
std::filesystem::path pc_opt_stamp(const std::filesystem::path& data_dir);

// Starts the optimised build on a background thread when the C is waiting in data/opt; `exe_id` goes into the stamp.
// A log goes to data/opt-build.log. Safe to call when there is nothing to do.
void pc_opt_start(const std::filesystem::path& data_dir, const std::string& exe_id);

// -1 when idle, else 0-100.
extern std::atomic<int> pc_opt_progress;

}  // namespace st
