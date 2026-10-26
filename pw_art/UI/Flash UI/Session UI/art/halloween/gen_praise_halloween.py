"""Halloween background of the praise notification: the shape of pw_art/UI/Praise/gen_praise.py (panel, rim,
portrait frames) in the colours of the skin, without the separator under the text.

  python gen_praise_halloween.py

Writes pw/branches/r1117/Data/UI/Styles/Icons/Praise/praise_hw_bg.dds (512x256, the panel in the top 512x160,
shown at 0.625 = 320x100 px) and previews/praise_hw_bg.png.
"""
import math, os, struct, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..', '..', '..'))
OUT = os.path.join(ROOT, 'pw', 'branches', 'r1117', 'Data', 'UI', 'Styles', 'Icons', 'Praise')
SS = 4

DARK = (22, 9, 18)
RIM = (255, 128, 24)
RIM_DARK = (70, 26, 10)


def save_dds(img, path):
    # uncompressed A8R8G8B8, as gen_praise.py
    img = img.convert('RGBA')
    w, h = img.size
    flags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000
    pf = struct.pack('<II4sIIIII', 32, 0x41, b'\0\0\0\0', 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    header = struct.pack('<4sIIIIIII', b'DDS ', 124, flags, h, w, w * 4, 0, 0) + b'\0' * 44 + pf + struct.pack('<IIIII', 0x1000, 0, 0, 0, 0)
    r, g, b, a = img.split()
    with open(path, 'wb') as f:
        f.write(header + Image.merge('RGBA', (b, g, r, a)).tobytes())


TW, TH = 512, 256
PW, PH = 512, 160
K = 1.6
S = SS
img = Image.new('RGBA', (TW * S, TH * S), (0, 0, 0, 0))

sh = Image.new('RGBA', img.size, (0, 0, 0, 0))
ImageDraw.Draw(sh).rounded_rectangle([6 * S, 8 * S, (PW - 4) * S, (PH - 2) * S], radius=22 * S, fill=(0, 0, 0, 110))
sh = sh.filter(ImageFilter.GaussianBlur(6 * S))
img = Image.alpha_composite(img, sh)

# body: vertical purple gradient, lighter in the middle band (the purple of the talents / statistics windows)
grad = Image.new('RGBA', (1, PH), (0, 0, 0, 0))
gp = grad.load()
for y in range(PH):
    t = y / (PH - 1.0)
    mid = math.exp(-((t - 0.42) / 0.33) ** 2)
    r = int(30 + (56 - 30) * mid); g = int(12 + (28 - 12) * mid); b = int(36 + (66 - 36) * mid)
    gp[0, y] = (r, g, b, 228)
grad = grad.resize((PW * S, PH * S), Image.BICUBIC)
mask = Image.new('L', img.size, 0)
ImageDraw.Draw(mask).rounded_rectangle([4 * S, 4 * S, (PW - 4) * S, (PH - 4) * S], radius=20 * S, fill=255)
layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
layer.paste(grad, (0, 0))
layer.putalpha(ImageChops.multiply(layer.getchannel('A'), mask))
img = Image.alpha_composite(img, layer)

# glow of the rim inside the panel
glow = Image.new('RGBA', img.size, (0, 0, 0, 0))
ImageDraw.Draw(glow).rounded_rectangle([8 * S, 8 * S, (PW - 8) * S, (PH - 8) * S], radius=16 * S, outline=RIM + (150,), width=3 * S)
glow = glow.filter(ImageFilter.GaussianBlur(4 * S))
glow.putalpha(ImageChops.multiply(glow.getchannel('A'), mask))
img = Image.alpha_composite(img, glow)

d = ImageDraw.Draw(img)
d.rounded_rectangle([4 * S, 4 * S, (PW - 4) * S, (PH - 4) * S], radius=20 * S, outline=DARK + (255,), width=3 * S)
d.rounded_rectangle([7 * S, 7 * S, (PW - 7) * S, (PH - 7) * S], radius=17 * S, outline=RIM + (255,), width=int(1.5 * S))
d.rounded_rectangle([9.5 * S, 9.5 * S, (PW - 9.5) * S, (PH - 9.5) * S], radius=15 * S, outline=RIM_DARK + (200,), width=1 * S)

# portrait frames: 64x64 panel px at (10, 8) and (246, 8)
for px0 in (10, 246):
    x0, y0 = px0 * K, 8 * K
    x1, y1 = (px0 + 64) * K, (8 + 64) * K
    d.rounded_rectangle([(x0 - 2) * S, (y0 - 2) * S, (x1 + 2) * S, (y1 + 2) * S], radius=7 * S, fill=DARK + (255,), outline=RIM_DARK + (255,), width=2 * S)
    d.rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=6 * S, outline=RIM + (255,), width=int(1.5 * S))

img = img.resize((TW, TH), Image.LANCZOS)
save_dds(img, os.path.join(OUT, 'praise_hw_bg.dds'))
os.makedirs(os.path.join(HERE, 'previews'), exist_ok=True)
img.save(os.path.join(HERE, 'previews', 'praise_hw_bg.png'))
print('done')
