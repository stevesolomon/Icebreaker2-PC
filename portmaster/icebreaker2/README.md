## Notes

This is a PortMaster port of **Icebreaker 2** for Rocknix / ArkOS / AmberELEC
/ muOS / TheRA / EmuELEC handheld Linux devices.

Huge thanks to **Magnet Interactive Studios** for creating the original
Panasonic 3DO release in 1996, and to **Andy Eddy** and the rest of the level
designers for the wild puzzle pyramids that make this game what it is. The
original PC source was generously preserved by the community; this port adds
SDL2 graphics, audio and gamepad layers and bundles the Icebreaker 1 level
pack.

## Controls

| Button         | Action                                |
| -------------- | ------------------------------------- |
| D-Pad / L-Stick | Move tetrahedron                     |
| A              | Fire weapon                           |
| B              | Cancel / back                         |
| X              | Cycle weapon                          |
| Y              | Use special                           |
| L1 / R1        | Strafe                                |
| Start          | Pause                                 |
| Select         | Menu / back                           |
| Select + Start | Force-quit (PortMaster standard)      |

## Save data

Save data is stored inside the port folder at
`ports/icebreaker2/conf/saves/`. Each level pack and difficulty has its own
save slot; deleting a single file in there resets only that pack.

## Build (porter notes)

```shell
# Native Linux x86_64 (for testing on a desktop)
cmake -B build-linux -S . -G Ninja
cmake --build build-linux

# aarch64 cross-compile for handhelds
bash tools/setup_aarch64_sysroot.sh
cmake -B build-aarch64 -S . -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
      -DIB2_AARCH64_SYSROOT=$HOME/aarch64-sysroot/root
cmake --build build-aarch64
```

The resulting binary depends only on SDL2/SDL2_image/SDL2_mixer/SDL2_ttf
plus libstdc++ and libc — exactly what every PortMaster device already
ships.
