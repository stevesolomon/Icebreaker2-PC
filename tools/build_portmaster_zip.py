#!/usr/bin/env python3
"""Build the PortMaster zip with Unix permissions preserved."""
import os, stat, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC  = os.path.join(ROOT, "portmaster", "icebreaker2")
OUT  = os.path.join(ROOT, "portmaster", "icebreaker2.zip")

# Files/dirs that must be marked executable inside the zip.
EXEC = {"Icebreaker 2.sh", "icebreaker2/Icebreaker2.aarch64"}

# Text files we should normalize to LF before zipping.
TEXT = {"Icebreaker 2.sh", "README.md", "gameinfo.xml", "port.json",
        "icebreaker2/icebreaker2.gptk",
        "icebreaker2/licenses/LICENSE.txt",
        "icebreaker2/licenses/LICENSE-game.txt"}

if os.path.exists(OUT):
    os.remove(OUT)

with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for root, dirs, files in os.walk(SRC):
        dirs.sort()
        files.sort()
        for f in files:
            full = os.path.join(root, f)
            rel  = os.path.relpath(full, SRC).replace("\\", "/")

            if rel in TEXT:
                with open(full, "rb") as fh:
                    data = fh.read().replace(b"\r\n", b"\n")
            else:
                with open(full, "rb") as fh:
                    data = fh.read()

            zi = zipfile.ZipInfo(rel)
            zi.compress_type = zipfile.ZIP_DEFLATED
            mode = 0o755 if rel in EXEC else 0o644
            zi.external_attr = (mode << 16) | 0
            z.writestr(zi, data)

size = os.path.getsize(OUT)
print(f"Wrote {OUT} ({size} bytes, {size/1024/1024:.1f} MB)")

with zipfile.ZipFile(OUT) as z:
    names = z.namelist()
    print(f"  {len(names)} entries")
    for n in names[:8]:
        print(f"    {n}")
    print("    ...")
    for n in names[-3:]:
        print(f"    {n}")
