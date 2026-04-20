#!/bin/bash
set -e
SYSROOT=$HOME/aarch64-sysroot
mkdir -p "$SYSROOT/debs" "$SYSROOT/root"
cd "$SYSROOT/debs"
M=http://ports.ubuntu.com/ubuntu-ports/pool/main/libs/libsdl2
U=http://ports.ubuntu.com/ubuntu-ports/pool/universe/libs/libsdl2
URLS="
$M/libsdl2-2.0-0_2.0.20+dfsg-2ubuntu1.22.04.1_arm64.deb
$U/libsdl2-dev_2.32.10+dfsg-6_arm64.deb
$U-image/libsdl2-image-2.0-0_2.8.8+dfsg-2_arm64.deb
$U-image/libsdl2-image-dev_2.8.8+dfsg-2_arm64.deb
$U-mixer/libsdl2-mixer-2.0-0_2.8.1+dfsg-5_arm64.deb
$U-mixer/libsdl2-mixer-dev_2.8.1+dfsg-5_arm64.deb
$U-ttf/libsdl2-ttf-2.0-0_2.24.0+dfsg-3_arm64.deb
$U-ttf/libsdl2-ttf-dev_2.24.0+dfsg-3_arm64.deb
"
for url in $URLS; do
    f=$(basename "$url")
    if [ ! -f "$f" ]; then
        echo "Fetching $f"
        wget -q "$url" || { echo "FAIL: $url"; exit 1; }
    fi
done
echo "--- Extracting to $SYSROOT/root ---"
rm -rf "$SYSROOT/root"
mkdir -p "$SYSROOT/root"
for d in *.deb; do
    dpkg-deb -x "$d" "$SYSROOT/root"
done

# The dev .deb ships /usr/lib/aarch64-linux-gnu/libSDL2.so as a symlink to the
# absolute path /usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0 which won't resolve
# inside our sysroot. Re-point each one to a relative target.
cd "$SYSROOT/root/usr/lib/aarch64-linux-gnu"
for s in libSDL2.so libSDL2_image.so libSDL2_mixer.so libSDL2_ttf.so; do
    if [ -L "$s" ]; then
        # Point at the SONAME (.so.0) so we don't depend on a specific
        # micro-version file existing in the sysroot.
        soname=$(echo "$s" | sed 's/\.so$/-2.0.so.0/')
        if [ "$s" = "libSDL2.so" ]; then soname=libSDL2-2.0.so.0; fi
        rm -f "$s"
        ln -s "$soname" "$s"
        echo "Re-linked $s -> $soname"
    fi
done

echo "--- Sysroot SDL2 libs ---"
ls -la "$SYSROOT/root/usr/lib/aarch64-linux-gnu/" | grep -E "libSDL2"
echo "DONE: sysroot at $SYSROOT/root"

