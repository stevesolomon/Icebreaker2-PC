# PortMaster port — Icebreaker 2

This directory contains the assembled PortMaster v2 port of Icebreaker 2.

## Layout

```
portmaster/
├── icebreaker2.zip                       ← the shippable artefact
└── icebreaker2/
    ├── port.json                         port metadata (PortMaster v2 schema)
    ├── README.md                         user-facing readme
    ├── gameinfo.xml                      EmulationStation metadata
    ├── screenshot.png                    placeholder; replace with real capture
    ├── Icebreaker 2.sh                   launcher (must keep the space + caps)
    └── icebreaker2/
        ├── Icebreaker2.aarch64           cross-compiled binary
        ├── icebreaker2.gptk              gptokeyb (mostly disabled)
        ├── assets/                       full game assets tree
        └── licenses/                     LICENSE-game.txt + LICENSE.txt
```

## Rebuilding the zip from a fresh clone

```bash
# 1. Cross-compile the binary
bash tools/setup_aarch64_sysroot.sh
cmake -B build-aarch64 -S . -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
cmake --build build-aarch64

# 2. Stage the binary + assets into the port tree
cp build-aarch64/Icebreaker2 portmaster/icebreaker2/icebreaker2/Icebreaker2.aarch64
cp -R assets portmaster/icebreaker2/icebreaker2/assets

# 3. Build the zip with proper Unix perms + line endings
python3 tools/build_portmaster_zip.py
# → portmaster/icebreaker2.zip
```

## Submitting to PortMaster

This repo only produces the zip artefact. To get it listed in the official
PortMaster catalogue, open a PR against
[PortsMaster-MR-New](https://github.com/PortsMaster/PortMaster-New) per the
contribution guide at <https://portmaster.games/packaging.html>.
