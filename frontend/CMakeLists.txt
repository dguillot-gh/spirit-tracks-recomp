# Spirit Tracks: the recompiled game (st_game, from the player's ROM) and the program (SpiritTracks).
# The recompiler's config from the ROM's header: game code at 0x0C (BKIE = USA, BKIP = Europe) and, for Europe, the ROM
# version at 0x1E (Revision 0: config/bkip, Revision 1: config/bkip1; their overlays differ).
set(ST_CONFIG_CODE "bkie")
if(ST_ROM AND EXISTS "${ST_ROM}")
  file(READ "${ST_ROM}" ST_ROM_CODE_HEX OFFSET 12 LIMIT 4 HEX)
  file(READ "${ST_ROM}" ST_ROM_VERSION_HEX OFFSET 30 LIMIT 1 HEX)
  if(ST_ROM_CODE_HEX STREQUAL "424b4950")      # BKIP (Europe)
    if(ST_ROM_VERSION_HEX STREQUAL "00")
      set(ST_CONFIG_CODE "bkip")
    else()
      set(ST_CONFIG_CODE "bkip1")
    endif()
  endif()
  message(STATUS "Spirit Tracks config: ${ST_CONFIG_CODE}")
endif()
set(ST_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/config/${ST_CONFIG_CODE})
# The version and commit the program shows (menu header, window title): "v0.1.0 (abc1234)", "-dirty" with local changes.
execute_process(COMMAND git rev-parse --short=7 HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE ST_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git status --porcelain --untracked-files=no WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE ST_GIT_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT ST_GIT_COMMIT)
  set(ST_GIT_COMMIT "unknown")
elseif(ST_GIT_DIRTY)
  string(APPEND ST_GIT_COMMIT "-dirty")
endif()
# CI passes -DST_VERSION from the release tag (v0.2.1 -> 0.2.1), so the exe says what the Release says; local builds
# show the project's version.
set(ST_VERSION "${PROJECT_VERSION}" CACHE STRING "Version the program shows (CI: the release tag)")
set(ST_BUILD_LABEL "v${ST_VERSION} (${ST_GIT_COMMIT})")
if(EXISTS ${CMAKE_SOURCE_DIR}/.git)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/.git)
endif()

# Embedded HD font (Inter Bold, SIL OFL 1.1) for optional vector text rendering.
set(ST_HD_FONT_SRC ${CMAKE_CURRENT_BINARY_DIR}/hd_font_data.cpp)
add_custom_command(
  OUTPUT ${ST_HD_FONT_SRC}
  COMMAND ${CMAKE_COMMAND}
          -D INPUT=${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/Inter-Bold.ttf
          -D OUTPUT=${ST_HD_FONT_SRC}
          -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFont.cmake
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/Inter-Bold.ttf ${CMAKE_SOURCE_DIR}/cmake/EmbedFont.cmake
  COMMENT "Embedding HD font (Inter-Bold.ttf)"
  VERBATIM)

# ---- Android: the game's C comes from a PC build (recomp_generated), compiled here for the phone ----
if(ANDROID)
  set(ST_GENERATED_DIR "" CACHE PATH "The recomp_generated folder of a PC build (2-BUILD.ps1)")
  if(ST_SHELL)
    # The shell APK: no game inside. libstbuild = the recompiler + TinyCC (LGPL, kept in its own library), which build
    # files/game/libst_game.so on the phone from the player's ROM (android_build.cpp).
    set(TCC_DIR ${CMAKE_SOURCE_DIR}/third_party/tinycc)
    add_library(stbuild SHARED src/android_build.cpp src/game_builder.cpp ${TCC_DIR}/libtcc.c)
    set_source_files_properties(${TCC_DIR}/libtcc.c PROPERTIES COMPILE_OPTIONS "-w" COMPILE_DEFINITIONS "ONE_SOURCE=1")
    target_include_directories(stbuild PRIVATE ${TCC_DIR}/config/android ${TCC_DIR})
    target_compile_options(stbuild PRIVATE -O2)
    target_link_libraries(stbuild PRIVATE ntr_recomp ntr_common log)
  elseif(NOT EXISTS "${ST_GENERATED_DIR}/func_table.c")
    message(FATAL_ERROR "ST_GENERATED_DIR must be a PC build's recomp_generated folder (run 2-BUILD.ps1 first)")
  endif()
  file(GLOB ST_GEN_SHARDS CONFIGURE_DEPENDS "${ST_GENERATED_DIR}/funcs_*.c")
  if(NOT ST_SHELL)
  add_library(st_game SHARED ${ST_GENERATED_DIR}/func_table.c ${ST_GEN_SHARDS})
  target_include_directories(st_game PRIVATE ${ST_GENERATED_DIR} ${CMAKE_SOURCE_DIR}/runtime/include)
  target_compile_definitions(st_game PRIVATE NTR_GAME_MODULE=1)
  set_target_properties(st_game PROPERTIES C_VISIBILITY_PRESET hidden)
  target_compile_options(st_game PRIVATE -w -ffreestanding -fno-stack-protector -O2)
  target_link_options(st_game PRIVATE -nostdlib)
  endif()

  # SDL's Android activity loads the program as libmain.so and calls its SDL_main.
  add_library(main SHARED src/main.cpp src/game_module.cpp src/st_hooks.cpp src/st_stick.cpp src/st_pad.cpp src/st_text.cpp
              ${ST_HD_FONT_SRC} src/png.cpp src/android_bridge.cpp src/gl_present.cpp src/gles_compat.cpp)
  target_include_directories(main PRIVATE src ${CMAKE_SOURCE_DIR}/third_party/stb)
  target_compile_options(main PRIVATE -O2)
  target_link_libraries(main PRIVATE ntr_runtime ntr_common SDL3::SDL3-static android log
                        "$<LINK_GROUP:RESCAN,ntr_runtime,melonds_core>")
  if(NOT ST_SHELL)
    add_dependencies(main st_game)
  endif()
  return()
endif()

# ---- the recompiled game: built only with ST_ROM (it is generated from the player's own ROM, never committed) ----
if(ST_ROM)
  set(ST_GEN_DIR ${CMAKE_BINARY_DIR}/recomp_generated)
  # ntr-recomp writes func_table.c and 64 shards funcs_000.c .. funcs_063.c (plus funcs.h).
  set(ST_GEN_SOURCES ${ST_GEN_DIR}/func_table.c)
  foreach(i RANGE 63)
    if(i LESS 10)
      set(n "00${i}")
    else()
      set(n "0${i}")
    endif()
    list(APPEND ST_GEN_SOURCES ${ST_GEN_DIR}/funcs_${n}.c)
  endforeach()

  add_custom_command(
    OUTPUT ${ST_GEN_SOURCES} ${ST_GEN_DIR}/funcs.h
    COMMAND ntr-recomp --rom "${ST_ROM}" --config "${ST_CONFIG}" --out "${ST_GEN_DIR}"
            --report "${CMAKE_BINARY_DIR}/recomp_report.txt"
    DEPENDS ntr-recomp ${ST_CONFIG}/functions.txt ${ST_CONFIG}/hooks.txt ${ST_CONFIG}/regions.txt "${ST_ROM}"
    COMMENT "Recompiling the game's code from your ROM"
    VERBATIM)
  # Only the C (what the Android build needs): cmake --build --preset release --target ph_generate
  add_custom_target(ph_generate DEPENDS ${ST_GEN_SOURCES} ${ST_GEN_DIR}/funcs.h)

  # A freestanding shared library: the runtime's services reach it through a table when it is loaded (ntr/recomp.h).
  add_library(st_game MODULE ${ST_GEN_SOURCES})
  target_include_directories(st_game PRIVATE ${ST_GEN_DIR} ${CMAKE_SOURCE_DIR}/runtime/include)
  target_compile_definitions(st_game PRIVATE NTR_GAME_MODULE=1)
  set_target_properties(st_game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    C_VISIBILITY_PRESET hidden)
  set(ST_GAME_OPT "" CACHE STRING "Optimization flag for the recompiled game (default /O1 or -O1)")
  if(MSVC)
    if(NOT ST_GAME_OPT)
      set(ST_GAME_OPT /O1)
    endif()
    target_compile_options(st_game PRIVATE /w /GS- /Zl ${ST_GAME_OPT})
    target_link_options(st_game PRIVATE /NODEFAULTLIB /NOENTRY)
  else()
    if(NOT ST_GAME_OPT)
      set(ST_GAME_OPT -O1)
    endif()
    target_compile_options(st_game PRIVATE -w -ffreestanding -fno-stack-protector ${ST_GAME_OPT})
    target_link_options(st_game PRIVATE -nostdlib)
  endif()
else()
  message(STATUS "ST_ROM not set: building the program only (it needs st_game next to it to play)")
endif()

# ---- the program ----
add_executable(SpiritTracks src/main.cpp src/game_module.cpp src/st_hooks.cpp src/st_stick.cpp src/st_pad.cpp src/st_text.cpp
               ${ST_HD_FONT_SRC} src/png.cpp src/gl_present.cpp src/menu.cpp)
target_compile_definitions(SpiritTracks PRIVATE "ST_BUILD_LABEL=\"${ST_BUILD_LABEL}\"")
target_include_directories(SpiritTracks PRIVATE src ${CMAKE_SOURCE_DIR}/third_party/stb)
set_target_properties(SpiritTracks PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
if(ST_ROM)
  add_dependencies(SpiritTracks st_game)  # (built together, next to the program)
endif()
target_link_libraries(SpiritTracks PRIVATE ntr_runtime ntr_common SDL3::SDL3-static)

# ---- first-run setup: the program builds st_game from the player's ROM itself (game_builder.cpp: the recompiler +
# TinyCC, LGPL), into the data folder. Its headers and config are packed into the program. ----
if(WIN32)
  set(TCC_DIR ${CMAKE_SOURCE_DIR}/third_party/tinycc)
  target_sources(SpiritTracks PRIVATE src/game_builder.cpp src/win_pick.cpp src/pc_launcher.cpp ${TCC_DIR}/libtcc.c)
  target_link_libraries(SpiritTracks PRIVATE comdlg32)
  set_source_files_properties(${TCC_DIR}/libtcc.c PROPERTIES COMPILE_OPTIONS "-w" COMPILE_DEFINITIONS "ONE_SOURCE=1")
  set_source_files_properties(src/game_builder.cpp PROPERTIES INCLUDE_DIRECTORIES "${TCC_DIR}")
  target_include_directories(SpiritTracks PRIVATE ${TCC_DIR}/config/windows)
  target_link_libraries(SpiritTracks PRIVATE ntr_recomp)
  target_compile_definitions(SpiritTracks PRIVATE ST_PC_SETUP=1)
  # The compiler's support files are packed into the exe (unpacked to the data folder only while building).
  set(ST_TOOLCHAIN_CPP ${CMAKE_CURRENT_BINARY_DIR}/embedded_toolchain_data.cpp)
  file(GLOB_RECURSE ST_TOOLCHAIN_INPUTS CONFIGURE_DEPENDS ${TCC_DIR}/include/* ${TCC_DIR}/win32/lib/*
       ${CMAKE_SOURCE_DIR}/runtime/include/* ${ST_CONFIG}/*)
  add_custom_command(OUTPUT ${ST_TOOLCHAIN_CPP}
    COMMAND ${CMAKE_COMMAND} -DOUT=${ST_TOOLCHAIN_CPP} -DTCC=${TCC_DIR} -DONDEVICE=${CMAKE_SOURCE_DIR}/tools/ondevice
            -DRT=${CMAKE_SOURCE_DIR}/runtime/include -DCFG=${ST_CONFIG} -P ${CMAKE_SOURCE_DIR}/tools/embed_toolchain.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/embed_toolchain.cmake ${ST_TOOLCHAIN_INPUTS}
            ${CMAKE_SOURCE_DIR}/tools/ondevice/stdint.h ${CMAKE_SOURCE_DIR}/tools/ondevice/string.h
            ${CMAKE_SOURCE_DIR}/tools/ondevice/dllstart.c
    VERBATIM)
  target_sources(SpiritTracks PRIVATE src/embedded_toolchain.cpp ${ST_TOOLCHAIN_CPP})
  set_source_files_properties(${ST_TOOLCHAIN_CPP} PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/src)
  # The optimised build (pc_optimizer.cpp): clang + lld packed into the exe too. ST_PC_CLANG_DIR: a folder with
  # clang.exe, ld.lld.exe, their DLLs, libmsvcrt.a, dllstart.c and chkstk_ms.S (CI: windows-exe.yml).
  set(ST_DATA_NAME "" CACHE STRING "PC data folder name under %APPDATA%\\ST-Recomp (empty: SpiritTracks; test builds: their own)")
  if(ST_DATA_NAME)
    target_compile_definitions(SpiritTracks PRIVATE "ST_DATA_NAME=\"${ST_DATA_NAME}\"")
  endif()
  set(ST_PC_CLANG_DIR "" CACHE PATH "Folder with the clang + lld to pack into the exe for the optimised game build")
  if(ST_PC_CLANG_DIR)
    enable_language(ASM)
    set(ST_CLANG_OUT ${CMAKE_CURRENT_BINARY_DIR}/clang_pack)
    file(MAKE_DIRECTORY ${ST_CLANG_OUT})
    file(GLOB ST_CLANG_INPUTS CONFIGURE_DEPENDS ${ST_PC_CLANG_DIR}/*)
    add_custom_command(OUTPUT ${ST_CLANG_OUT}/clang_pack_index.cpp ${ST_CLANG_OUT}/clang_pack.S ${ST_CLANG_OUT}/clang_pack.bin
      COMMAND ${CMAKE_COMMAND} -DDIR=${ST_PC_CLANG_DIR} -DOUT=${ST_CLANG_OUT} -P ${CMAKE_SOURCE_DIR}/tools/embed_clang.cmake
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/embed_clang.cmake ${ST_CLANG_INPUTS}
      VERBATIM)
    set_source_files_properties(${ST_CLANG_OUT}/clang_pack.S PROPERTIES OBJECT_DEPENDS ${ST_CLANG_OUT}/clang_pack.bin)
    set_source_files_properties(${ST_CLANG_OUT}/clang_pack_index.cpp PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_sources(SpiritTracks PRIVATE src/pc_optimizer.cpp ${ST_CLANG_OUT}/clang_pack_index.cpp ${ST_CLANG_OUT}/clang_pack.S)
    target_compile_definitions(SpiritTracks PRIVATE ST_PC_CLANG=1)
  endif()
endif()
if(ST_GLES_SHADERS)  # (the gles-test preset: the phone's renderer, its ES shaders and gles_compat, on the PC)
  target_sources(SpiritTracks PRIVATE src/gles_compat.cpp)
  target_compile_definitions(SpiritTracks PRIVATE ST_GLES_TEST=1)
endif()
if(NOT MSVC)
  # melonDS calls the Platform:: functions ntr_runtime defines and ntr_runtime calls melonDS: rescan them (GNU ld).
  target_link_libraries(SpiritTracks PRIVATE "$<LINK_GROUP:RESCAN,ntr_runtime,melonds_core>")
endif()
if(WIN32)
  enable_language(RC)
  target_sources(SpiritTracks PRIVATE app.rc)
  set_source_files_properties(app.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/app.ico)
  target_link_libraries(SpiritTracks PRIVATE opengl32 user32 advapi32 shell32 ws2_32 iphlpapi)
  # A windowed program (no console window behind it); its output still goes to a redirected stdout/stderr.
  if(MSVC)
    target_link_options(SpiritTracks PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  else()
    target_link_options(SpiritTracks PRIVATE -mwindows)
  endif()
endif()
