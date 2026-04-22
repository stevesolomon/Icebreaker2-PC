################################################################################
#  Icebreaker 2 — SDL2 Port (Windows + Linux/PortMaster)
#  CMakeLists.txt
#
#  Build with:
#    cmake -B build -S .
#    cmake --build build --config Release
#
#  Cross-compile for aarch64 (PortMaster handhelds):
#    cmake -B build-aarch64 -S . \
#          -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
#    cmake --build build-aarch64
#
#  Prerequisites:
#    - SDL2, SDL2_image, SDL2_mixer, SDL2_ttf
#      (vcpkg on Windows; libsdl2*-dev on Debian/Ubuntu)
#    - CMake 3.20+
#    - MSVC, MinGW-w64, GCC, or Clang
################################################################################
cmake_minimum_required(VERSION 3.20)
project(Icebreaker2 VERSION 6.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find SDL2 and extensions ─────────────────────────────────────────────────
# Three discovery modes:
#   (a) Cross-compiling with a sysroot → link directly using --sysroot
#       (sysroot already has libSDL2*.so + headers; transitive .pc deps on
#       libwebp/libpng/etc. would otherwise force pulling those into the
#       sysroot too, which is unnecessary because the target device provides
#       them at runtime).
#   (b) CONFIG mode (vcpkg / Windows / newer Linux distros).
#   (c) pkg-config (most Linux distros).
#   (d) Last-resort MODULE mode (CMake's bundled FindSDL2.cmake).
include(FindPkgConfig)

set(IB2_SDL_DIRECT_LINK FALSE)
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
    if(EXISTS "${CMAKE_SYSROOT}/usr/include/SDL2/SDL.h")
        set(IB2_SDL_DIRECT_LINK TRUE)
        message(STATUS "Cross-compile: using direct SDL2 linkage from ${CMAKE_SYSROOT}")
    endif()
endif()

if(NOT IB2_SDL_DIRECT_LINK)
    function(_ib2_find_sdl_component pretty_name pc_name config_name out_libs out_includes)
        find_package(${config_name} CONFIG QUIET)
        if(${config_name}_FOUND)
            message(STATUS "Found ${pretty_name} via CMake config")
            return()
        endif()
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(PC_${config_name} QUIET ${pc_name})
            if(PC_${config_name}_FOUND)
                set(${out_libs}     "${PC_${config_name}_LIBRARIES}"     PARENT_SCOPE)
                set(${out_includes} "${PC_${config_name}_INCLUDE_DIRS}"  PARENT_SCOPE)
                link_directories(${PC_${config_name}_LIBRARY_DIRS})
                message(STATUS "Found ${pretty_name} via pkg-config (${PC_${config_name}_VERSION})")
                return()
            endif()
        endif()
        find_package(${config_name} REQUIRED)
    endfunction()

    # SDL2 itself: try CONFIG, pkg-config, then MODULE.
    find_package(SDL2 CONFIG QUIET)
    if(NOT SDL2_FOUND AND PKG_CONFIG_FOUND)
        pkg_check_modules(PC_SDL2 QUIET sdl2)
        if(PC_SDL2_FOUND)
            set(SDL2_PC_LIBS     "${PC_SDL2_LIBRARIES}")
            set(SDL2_PC_INCLUDES "${PC_SDL2_INCLUDE_DIRS}")
            link_directories(${PC_SDL2_LIBRARY_DIRS})
            set(SDL2_FOUND TRUE)
            message(STATUS "Found SDL2 via pkg-config (${PC_SDL2_VERSION})")
        endif()
    endif()
    if(NOT SDL2_FOUND)
        find_package(SDL2 REQUIRED)
    endif()

    _ib2_find_sdl_component("SDL2_image" SDL2_image SDL2_image SDL2_IMAGE_PC_LIBS SDL2_IMAGE_PC_INCLUDES)
    _ib2_find_sdl_component("SDL2_mixer" SDL2_mixer SDL2_mixer SDL2_MIXER_PC_LIBS SDL2_MIXER_PC_INCLUDES)
    _ib2_find_sdl_component("SDL2_ttf"   SDL2_ttf   SDL2_ttf   SDL2_TTF_PC_LIBS   SDL2_TTF_PC_INCLUDES)
endif()

# ── Platform abstraction layer sources ───────────────────────────────────────
set(PLATFORM_SOURCES
    src/platform/graphics.cpp
    src/platform/input.cpp
    src/platform/timer.cpp
    src/platform/audio.cpp
    src/platform/filesystem.cpp
)

set(PLATFORM_HEADERS
    src/platform/platform.h
    src/platform/types.h
    src/platform/graphics.h
    src/platform/input.h
    src/platform/timer.h
    src/platform/audio.h
    src/platform/filesystem.h
)

# ── Asset loader sources ────────────────────────────────────────────────────
set(ASSET_SOURCES
    src/assets/cel_loader.cpp
    src/assets/anim_loader.cpp
)

set(ASSET_HEADERS
    src/assets/cel_loader.h
    src/assets/anim_loader.h
)

# ── Game source files (ported from .cp to .cpp) ─────────────────────────────
set(GAME_SOURCES
    src/icetwo.cpp
    src/landscape.cpp
    src/animation.cpp
    src/solids.cpp
    src/dudemeyer.cpp
    src/seeker.cpp
    src/weapon.cpp
    src/deadlist.cpp
    src/levels.cpp
    src/levels_ib1.cpp
    src/sounds.cpp
    src/userif.cpp
    src/FontHandler.cpp
    src/memcheck.cpp
    src/PlayMusic.cpp
    src/PrepareStream.cpp
    src/nvram.cpp
)

set(GAME_HEADERS
    src/icebreaker.h
    src/landscape.h
    src/animation.h
    src/solids.h
    src/dudemeyer.h
    src/seeker.h
    src/weapon.h
    src/deadlist.h
    src/levels.h
    src/levels_ib1.h
    src/sounds.h
    src/userif.h
    src/FontHandler.h
    src/memcheck.h
    src/PlayMusic.h
    src/PrepareStream.h
    src/nvram.h
)

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}
    ${PLATFORM_SOURCES}
    ${PLATFORM_HEADERS}
    ${ASSET_SOURCES}
    ${ASSET_HEADERS}
    ${GAME_SOURCES}
    ${GAME_HEADERS}
)

# ── Include paths ────────────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/platform
    ${CMAKE_SOURCE_DIR}/src/assets
)

# ── Windows: stage the runtime tree in its own subdirectory ─────────────────
# Without this the .exe + assets + every vcpkg-copied SDL2*.dll sit at the top
# of build/ alongside the build-system bookkeeping (CMakeFiles/, .ninja_deps,
# .pdb/.ilk, etc). Redirect the runtime artefacts into build/Icebreaker2/ so
# the runtime tree is self-contained and easy to zip up or share.
if(WIN32)
    set(IB2_RUNTIME_DIR "${CMAKE_BINARY_DIR}/Icebreaker2")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY         "${IB2_RUNTIME_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${IB2_RUNTIME_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${IB2_RUNTIME_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${IB2_RUNTIME_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${IB2_RUNTIME_DIR}"
    )
endif()

# ── Compiler definitions ─────────────────────────────────────────────────────
target_compile_definitions(${PROJECT_NAME} PRIVATE
    IB2_SDL_PORT=1
    SDL_MAIN_HANDLED
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_PORT=1            # legacy alias, kept for any in-tree references
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ── Link SDL2 libraries ─────────────────────────────────────────────────────
# Use the modern targets when available; fall back to the variables exported by
# the legacy FindSDL2*.cmake modules (Linux pkg-config path).
function(_ib2_link_sdl libname target_modern var_libs var_includes)
    if(IB2_SDL_DIRECT_LINK)
        # Direct linkage: --sysroot already provides include + lib paths;
        # we only need the -l flag.
        string(TOLOWER "${libname}" lower)
        if(lower STREQUAL "sdl2")
            target_link_libraries(${PROJECT_NAME} PRIVATE -lSDL2)
        else()
            target_link_libraries(${PROJECT_NAME} PRIVATE -l${libname})
        endif()
        target_include_directories(${PROJECT_NAME} PRIVATE
            "${CMAKE_SYSROOT}/usr/include/SDL2")
        return()
    endif()
    if(TARGET ${target_modern})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${target_modern})
    elseif(TARGET ${target_modern}-static)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${target_modern}-static)
    elseif(${var_libs})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${${var_libs}})
        target_include_directories(${PROJECT_NAME} PRIVATE ${${var_includes}})
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE ${${libname}_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${${libname}_INCLUDE_DIRS})
    endif()
endfunction()

_ib2_link_sdl(SDL2       SDL2::SDL2             SDL2_PC_LIBS         SDL2_PC_INCLUDES)
_ib2_link_sdl(SDL2_image SDL2_image::SDL2_image SDL2_IMAGE_PC_LIBS   SDL2_IMAGE_PC_INCLUDES)
_ib2_link_sdl(SDL2_mixer SDL2_mixer::SDL2_mixer SDL2_MIXER_PC_LIBS   SDL2_MIXER_PC_INCLUDES)
_ib2_link_sdl(SDL2_ttf   SDL2_ttf::SDL2_ttf     SDL2_TTF_PC_LIBS     SDL2_TTF_PC_INCLUDES)

# ── Platform-specific linking ────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        shell32    # SHGetFolderPath
    )
elseif(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads
        m            # math
        dl           # dlopen (SDL2 may use it)
    )
endif()

# ── Compiler warnings ───────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4996)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ── Copy assets to build directory ───────────────────────────────────────────
# An add_custom_target attached to ALL fires every build, even when the main
# executable is up-to-date. This avoids the gotcha where asset-only edits in
# `assets/` never propagate to `build/assets/` because the link step skipped
# the POST_BUILD command.
add_custom_target(sync_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Syncing game assets to build directory"
    VERBATIM
)
add_dependencies(sync_assets ${PROJECT_NAME})

# ── CEL2PNG converter tool (optional) ────────────────────────────────────────
# Uncomment when the converter source is ready:
# add_executable(cel2png tools/cel2png/main.cpp src/assets/cel_loader.cpp)
# target_include_directories(cel2png PRIVATE src/platform src/assets)
# target_link_libraries(cel2png PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)

# ── Installation ─────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION bin/assets)
