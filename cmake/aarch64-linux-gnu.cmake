# CMake toolchain file for cross-compiling Icebreaker 2 to aarch64 Linux
# (Rocknix / ArkOS / AmberELEC / muOS / etc — PortMaster handhelds).
#
# Usage:
#   cmake -B build-aarch64 -S . \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
#   cmake --build build-aarch64
#
# Requirements (Debian/Ubuntu):
#   sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake ninja-build
#   sudo dpkg --add-architecture arm64 && sudo apt update
#   sudo apt install libsdl2-dev:arm64 libsdl2-image-dev:arm64 \
#                    libsdl2-mixer-dev:arm64 libsdl2-ttf-dev:arm64
#
# Override the sysroot if you have a downloaded PortMaster sysroot tarball:
#   -DIB2_AARCH64_SYSROOT=/path/to/sysroot

set(CMAKE_SYSTEM_NAME       Linux)
set(CMAKE_SYSTEM_PROCESSOR  aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# If the user provided a sysroot, point CMake at it.
# Accept either -DIB2_AARCH64_SYSROOT=... or env IB2_AARCH64_SYSROOT.
if(NOT DEFINED IB2_AARCH64_SYSROOT AND DEFINED ENV{IB2_AARCH64_SYSROOT})
    set(IB2_AARCH64_SYSROOT "$ENV{IB2_AARCH64_SYSROOT}")
endif()

if(DEFINED IB2_AARCH64_SYSROOT)
    set(CMAKE_SYSROOT       "${IB2_AARCH64_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${IB2_AARCH64_SYSROOT}")
    list(APPEND CMAKE_PREFIX_PATH
        "${IB2_AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu/cmake"
        "${IB2_AARCH64_SYSROOT}/usr"
    )
    set(ENV{PKG_CONFIG_LIBDIR}
        "${IB2_AARCH64_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${IB2_AARCH64_SYSROOT}/usr/share/pkgconfig")
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${IB2_AARCH64_SYSROOT}")
else()
    # Fall back to Debian multi-arch paths on the host filesystem.
    list(APPEND CMAKE_PREFIX_PATH /usr/lib/aarch64-linux-gnu /usr/aarch64-linux-gnu)
    set(ENV{PKG_CONFIG_PATH}
        "/usr/lib/aarch64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    set(ENV{PKG_CONFIG_LIBDIR}
        "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# PortMaster handhelds dynamically link to libSDL2 from the device; we don't
# need rpaths or static linking. Keep the binary small and leave SDL2 as a
# runtime dependency.
#
# Tell the linker to ignore unresolved symbols inside shared libraries we
# link against (e.g. libSDL2.so → libX11/libtiff/libwebp). These symbols
# will resolve at runtime against the target device's own copies of those
# libraries; pulling them all into our build sysroot is unnecessary.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${CMAKE_EXE_LINKER_FLAGS_INIT} -Wl,--unresolved-symbols=ignore-in-shared-libs")
