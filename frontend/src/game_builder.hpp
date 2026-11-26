// Builds the game module from the player's ROM on their own device: the recompiler (ROM → C) then TinyCC (C → module).
// Used by the shell APK (android_build.cpp) and the PC program's first-run setup (main.cpp).
#pragma once

#include <atomic>
#include <string>

namespace st {

// `assets`: tcc/include (TinyCC's headers + stdint.h/string.h), tcc/lib (Windows: msvcrt.def, chkstk.S, dllstart.c),
// rt (runtime/include), config (the game's recompiler config). `work` is scratch (~200 MB, removed after).
// Returns "" on success, else a message for the player. `progress` goes 0-100.
// `keep_c` (optional): where the generated C goes after the build instead of being deleted, for the optimised
// rebuild with clang (Android: GameOptimizer.java).
std::string build_game(const std::string& rom_path, const std::string& assets, const std::string& out_module,
                       const std::string& work, std::atomic<int>& progress, const std::string& keep_c = "");

}  // namespace st
