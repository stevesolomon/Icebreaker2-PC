#!/bin/bash
# Icebreaker 2 — PortMaster launcher
# Targets: Rocknix / ArkOS / AmberELEC / muOS / TheRA / EmuELEC

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt

[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

get_controls

GAMEDIR="/$directory/ports/icebreaker2"
CONFDIR="$GAMEDIR/conf"

mkdir -p "$CONFDIR/saves"
cd "$GAMEDIR"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# Manual file copies (e.g. SMB / Windows Explorer) often strip the Unix
# executable bit. Restore it defensively so the launcher always works
# regardless of how the user got the files onto the device.
chmod +x "$GAMEDIR/Icebreaker2.${DEVICE_ARCH}" 2>/dev/null

# Icebreaker 2 reads this env var (added in the SDL port) to keep saves
# inside the port directory rather than in $XDG_DATA_HOME on the system root.
export IB2_SAVE_DIR="$CONFDIR/saves"

# Force native-panel fullscreen on handhelds (handled in InitGraphics).
export IB2_FULLSCREEN=1

# Pin asset lookup to the port directory so the binary doesn't depend on
# its current working directory at launch time.
export IB2_ASSETS_DIR="$GAMEDIR/assets"

# Per-port libraries (none today, but reserved for future SO bundles).
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# Help SDL pick a working audio backend on Rocknix devices. Most handhelds
# expose ALSA via tinyalsa; pulseaudio is rarely running. Try alsa first
# and fall back to pipewire if SDL was built with that support.
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"

# Use the gptokeyb launch wrapper for proper process tracking + the standard
# Select+Start force-quit chord. The .gptk maps everything else to "disabled"
# because the game already speaks SDL_GameController natively.
$GPTOKEYB "Icebreaker2.${DEVICE_ARCH}" -c "./icebreaker2.gptk" &
pm_platform_helper "$GAMEDIR/Icebreaker2.${DEVICE_ARCH}"

./Icebreaker2.${DEVICE_ARCH}

pm_finish
