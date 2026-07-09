#!/usr/bin/env python3
"""Build the PortMaster zip with Unix permissions preserved."""
import os, re, stat, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC  = os.path.join(ROOT, "portmaster", "icebreaker2")

def _read_version():
    cml = os.path.join(ROOT, "CMakeLists.txt")
    with open(cml, "r", encoding="utf-8") as fh:
        for line in fh:
            m = re.search(r"VERSION\s+([0-9][0-9.]*)", line)
            if m:
                return m.group(1)
    return "0.0.0"

VERSION = _read_version()
OUT  = os.path.join(ROOT, "portmaster", f"icebreaker2-{VERSION}-portmaster.zip")

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
