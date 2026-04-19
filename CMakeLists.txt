################################################################################
#  Icebreaker 2 — Windows Port
#  CMakeLists.txt
#
#  Build with:
#    cmake -B build -S .
#    cmake --build build --config Release
#
#  Prerequisites:
#    - SDL2, SDL2_image, SDL2_mixer, SDL2_ttf (via vcpkg or manual install)
#    - CMake 3.20+
#    - MSVC or MinGW-w64
################################################################################
cmake_minimum_required(VERSION 3.20)
project(Icebreaker2 VERSION 6.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find SDL2 and extensions ─────────────────────────────────────────────────
find_package(SDL2       REQUIRED CONFIG)
find_package(SDL2_image REQUIRED CONFIG)
find_package(SDL2_mixer REQUIRED CONFIG)
find_package(SDL2_ttf   REQUIRED CONFIG)

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

# ── Compiler definitions ─────────────────────────────────────────────────────
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_PORT=1
    WIN32_LEAN_AND_MEAN
    SDL_MAIN_HANDLED
    _CRT_SECURE_NO_WARNINGS
)

# ── Link SDL2 libraries ─────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
)

# ── Windows-specific linking ─────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        shell32    # SHGetFolderPath
    )
endif()

# ── Compiler warnings ───────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4996)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ── Copy assets to build directory ───────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying game assets to build directory"
)

# ── CEL2PNG converter tool (optional) ────────────────────────────────────────
# Uncomment when the converter source is ready:
# add_executable(cel2png tools/cel2png/main.cpp src/assets/cel_loader.cpp)
# target_include_directories(cel2png PRIVATE src/platform src/assets)
# target_link_libraries(cel2png PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)

# ── Installation ─────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION bin/assets)
