#!/usr/bin/env python3
"""
Generate a placeholder PortMaster screenshot for Icebreaker 2.

Output: portmaster/icebreaker2/screenshot.png at 640x480 (4:3) using
the game's own Helvetica.ttf font. The user is encouraged to replace
this with a real in-game capture once the port is running on hardware.
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "iso_assets", "helvetica.ttf")
if not os.path.exists(FONT):
    FONT = os.path.join(ROOT, "build", "assets", "MetaArt", "Helvetica.ttf")
OUT  = os.path.join(ROOT, "portmaster", "icebreaker2", "screenshot.png")

W, H = 640, 480

img = Image.new("RGB", (W, H), (8, 16, 48))
draw = ImageDraw.Draw(img)

# Soft radial-ish backdrop using overlapping translucent rectangles.
overlay = Image.new("RGBA", (W, H), (0, 0, 0, 0))
od = ImageDraw.Draw(overlay)
for r, alpha in [(420, 50), (320, 60), (220, 80), (120, 100)]:
    od.ellipse([(W//2 - r, H//2 - r), (W//2 + r, H//2 + r)],
               fill=(40, 90, 200, alpha))
img = Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB")
draw = ImageDraw.Draw(img)

big   = ImageFont.truetype(FONT, 92)
small = ImageFont.truetype(FONT, 28)
tiny  = ImageFont.truetype(FONT, 20)

def centered(text, font, y, fill):
    bbox = draw.textbbox((0, 0), text, font=font)
    w = bbox[2] - bbox[0]
    draw.text(((W - w) // 2, y), text, font=font, fill=fill)

centered("ICEBREAKER 2", big,    140, (240, 245, 255))
centered("Magnet Interactive Studios  ·  1996", small,  240, (200, 215, 235))
centered("PortMaster edition", small,  280, (180, 200, 220))
centered("Replace with a real gameplay capture before publishing.",
         tiny,  430, (140, 160, 190))

img.save(OUT, optimize=True)
print(f"Wrote {OUT} ({os.path.getsize(OUT)} bytes)")
