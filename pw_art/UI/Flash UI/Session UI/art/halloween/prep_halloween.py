# Prepares the halloween skin bitmaps for main.fla.
#   python prep_halloween.py
# Input:  generated/*.png (art on a green #00FF00 background)
# Output: final/HW_*.png (2x of the size shown in the game; the code draws them at 0.5)
import os
import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'generated')
OUT = os.path.join(HERE, 'final')
GLOW = {'pumpkin_on.png', 'header.png', 'close.png', 'frame.png', 'button_plank_hover.png', 'button_plank_active.png'}


def key(name):
    """Removes the green background: alpha from the green excess, colour unmixed from the background."""
    img = Image.open(os.path.join(SRC, name))
    a = np.asarray(img.convert('RGB')).astype(np.float32)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    border = np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]])
    gm = border[(border[:, 1] - np.maximum(border[:, 0], border[:, 2])) > 100]
    bg = np.median(gm, axis=0) if len(gm) else np.array([20, 240, 25], np.float32)
    spill = g - np.maximum(r, b)
    full = bg[1] - max(bg[0], bg[2])
    alpha = 1.0 - np.clip((spill - 0.12 * full) / (0.80 * full), 0, 1)
    A = alpha[..., None]
    F = np.clip((a - (1 - A) * bg) / np.maximum(A, 1e-3), 0, 255)
    F[..., 1] = np.where(spill > 0, np.minimum(F[..., 1], np.maximum(F[..., 0], F[..., 2])), F[..., 1])
    if name in GLOW:
        soft = alpha < 0.98
        F[..., 1] = np.where(soft, np.minimum(F[..., 1], 0.62 * F[..., 0]), F[..., 1])
    F[alpha < 0.01] = 0
    out = Image.fromarray(np.concatenate([F, A * 255], axis=2).round().astype(np.uint8), 'RGBA')
    bbox = out.getchannel('A').point(lambda v: 255 if v > 40 else 0).getbbox()
    return out.crop(bbox)


def resize(img, w, h):
    # premultiplied resize, so transparent pixels do not darken the edges
    a = np.asarray(img).astype(np.float32) / 255.0
    pm = a.copy()
    pm[..., :3] *= pm[..., 3:4]
    res = []
    for c in range(4):
        ch = Image.fromarray((pm[..., c] * 65535).astype(np.uint16).astype(np.int32), 'I')
        res.append(np.asarray(ch.resize((w, h), Image.LANCZOS)).astype(np.float32) / 65535)
    r = np.clip(np.stack(res, axis=2), 0, 1)
    al = r[..., 3:4]
    r[..., :3] = np.where(al > 1e-4, r[..., :3] / np.maximum(al, 1e-4), 0)
    return Image.fromarray((np.clip(r, 0, 1) * 255).round().astype(np.uint8), 'RGBA')


def scale(img, k):
    return resize(img, max(1, round(img.width * k)), max(1, round(img.height * k)))


def bright(img, k):
    rgb = ImageEnhance.Brightness(img.convert('RGB')).enhance(k)
    rgb.putalpha(img.getchannel('A'))
    return rgb


def disabled(img):
    rgb = ImageEnhance.Color(img.convert('RGB')).enhance(0.15)
    rgb = ImageEnhance.Brightness(rgb).enhance(0.6)
    rgb.putalpha(img.getchannel('A'))
    return rgb


def glow(img, color=(255, 140, 30), radius=6, strength=0.9):
    """Adds an outer orange glow (the image gets a margin of 'radius' pixels)."""
    m = radius * 2
    base = Image.new('RGBA', (img.width + m * 2, img.height + m * 2), (0, 0, 0, 0))
    base.paste(img, (m, m))
    al = base.getchannel('A').filter(ImageFilter.GaussianBlur(radius))
    al = al.point(lambda v: min(255, int(v * strength * 1.6)))
    g = Image.new('RGBA', base.size, color + (0,))
    g.putalpha(al)
    return Image.alpha_composite(g, base)


def save(img, name):
    img.save(os.path.join(OUT, name + '.png'))
    print(name, img.size)


def main():
    os.makedirs(OUT, exist_ok=True)

    # window background: opaque wood
    wood = Image.open(os.path.join(SRC, 'wood_back.png')).convert('RGB').resize((1024, 1024), Image.LANCZOS)
    wood = ImageEnhance.Brightness(wood).enhance(0.7)
    wood.save(os.path.join(OUT, 'HW_Wood.png'))
    print('HW_Wood', wood.size)

    # window frame: 4 corners, 4 plain strips (stretched), 4 ornaments (centred on the sides)
    fr = key('frame.png')
    W, H = fr.size
    T = 135                      # band thickness in the source
    C = 262                      # corner size in the source
    k = 28.0 / T                 # band = 14 px in the game (28 in 2x)
    save(scale(fr.crop((0, 0, C, C)), k), 'HW_FrameTL')
    save(scale(fr.crop((W - C, 0, W, C)), k), 'HW_FrameTR')
    save(scale(fr.crop((0, H - C, C, H)), k), 'HW_FrameBL')
    save(scale(fr.crop((W - C, H - C, W, H)), k), 'HW_FrameBR')
    save(resize(fr.crop((285, 0, 345, T)), 12, 28), 'HW_FrameT')
    save(resize(fr.crop((285, H - T, 345, H)), 12, 28), 'HW_FrameB')
    save(resize(fr.crop((0, 275, T, 330)), 28, 12), 'HW_FrameL')
    save(resize(fr.crop((W - T, 275, W, 330)), 28, 12), 'HW_FrameR')
    save(scale(fr.crop((368, 0, 818, T)), k), 'HW_FrameOT')
    save(scale(fr.crop((352, H - T, 812, H)), k), 'HW_FrameOB')
    save(scale(fr.crop((0, 336, T, 862)), k), 'HW_FrameOL')
    save(scale(fr.crop((W - T, 342, W, 858)), k), 'HW_FrameOR')

    # window title plate: left cap, middle (stretched), right cap; 28 px high in the game
    hd = key('header.png')
    k = 56.0 / hd.height
    cap = 210
    save(scale(hd.crop((0, 0, cap, hd.height)), k), 'HW_HeaderL')
    save(resize(hd.crop((800, 0, 960, hd.height)), 16, 56), 'HW_HeaderM')
    save(scale(hd.crop((hd.width - cap, 0, hd.width, hd.height)), k), 'HW_HeaderR')

    bat = key('bat.png')
    save(resize(bat, 152, round(bat.height * 152 / bat.width)), 'HW_Bat')
    web = key('cobweb_corner.png')
    save(resize(web, 128, round(web.height * 128 / web.width)), 'HW_Web')

    # standard button: plank; normal / hover / active (pressed) are drawn, disabled is the grey normal one
    def plank(name):
        b = key(name)
        return resize(b, 340, round(b.height * 340 / b.width))
    up = plank('button_plank_normal.png')
    save(up, 'HW_BtnUp')
    save(plank('button_plank_hover.png'), 'HW_BtnOver')
    save(plank('button_plank_active.png'), 'HW_BtnDown')
    save(disabled(up), 'HW_BtnDisable')

    cl = key('close.png')
    cu = resize(cl, 56, round(cl.height * 56 / cl.width))
    save(cu, 'HW_CloseUp')
    save(bright(cu, 1.3), 'HW_CloseOver')
    save(bright(cu, 0.8), 'HW_CloseDown')

    # checkbox: the same scale for both pumpkins, so that the bodies match
    off = key('pumpkin_off.png')
    on = key('pumpkin_on.png')
    k = 52.0 / off.height
    po, pn = scale(off, k), scale(on, k)
    save(po, 'HW_ChkOff')
    save(bright(po, 1.35), 'HW_ChkOffOver')
    save(bright(po, 0.8), 'HW_ChkOffDown')
    save(disabled(po), 'HW_ChkOffDisable')
    save(pn, 'HW_ChkOn')
    save(bright(pn, 1.15), 'HW_ChkOnOver')
    save(bright(pn, 0.8), 'HW_ChkOnDown')
    save(disabled(pn), 'HW_ChkOnDisable')

    # slider knob: candy corn, 24 px high in the game
    cc = key('candy_corn.png')
    ku = resize(cc, round(cc.width * 48 / cc.height), 48)
    save(ku, 'HW_KnobUp')
    save(glow(bright(ku, 1.1), radius=4), 'HW_KnobOver')
    save(bright(ku, 0.85), 'HW_KnobDown')

    # slider track: plank with pointed ends; 12 px high in the game
    pl = key('plank.png')
    k = 24.0 / pl.height
    save(scale(pl.crop((0, 0, 120, pl.height)), k), 'HW_PlankL')
    save(resize(pl.crop((560, 0, 660, pl.height)), 20, 24), 'HW_PlankM')
    save(scale(pl.crop((pl.width - 120, 0, pl.width, pl.height)), k), 'HW_PlankR')

    # section divider: thin branch, 480x16 in the game
    br = key('branch_line.png')
    save(resize(br, 960, 32), 'HW_Branch')

    # dark band behind a section title (made here, not generated)
    w, h = 960, 70
    x = np.linspace(0, 1, w)[None, :]
    y = np.linspace(-1, 1, h)[:, None]
    al = 0.65 * (1 - x) ** 1.3 * np.clip(1.4 - np.abs(y) ** 4 * 1.4, 0, 1)
    band = np.zeros((h, w, 4), np.float32)
    band[..., 0], band[..., 1], band[..., 2] = 22, 8, 28
    band[..., 3] = al * 255
    save(Image.fromarray(band.round().astype(np.uint8), 'RGBA'), 'HW_HeaderBand')

    # 9-slice plates drawn 1:1 by ScaleBitmap (not 2x): field of a value (key, slot; grid of textFrame.png 11,7,61,3)
    # and the drop-down list of a combobox (grid of frameChat.png 10,10,80,80)
    save(plate(85, 19, 5, (30, 12, 24, 235), (255, 138, 42, 210)), 'HW_ValueFrame')
    save(plate(100, 100, 7, (24, 10, 20, 240), (255, 128, 24, 230)), 'HW_ListBack')
    # background of the plain windows (talents, statistics), stretched to the window:
    # bitmaps, a Shape inside the window frame is not drawn by the game's player
    save(Image.new('RGBA', (16, 16), (46, 22, 56, 199)), 'HW_Plain')
    # inner panels (Components.Frame, frame_png 100x100, grid 10,10,80,80): semi-transparent purple, faint orange edge
    save(plate(100, 100, 8, (46, 22, 56, 170), (255, 138, 42, 110)), 'HW_Panel')
    # rows of the drop-down list (rowOver.png / rowSelected.png are 135x28): orange instead of teal
    save(row_light(135, 28, 0.45), 'HW_RowOver')
    save(row_light(135, 28, 0.75), 'HW_RowSelected')


def row_light(w, h, strength):
    """horizontal orange band, soft at the edges"""
    x = np.minimum(np.linspace(0, 1, w), np.linspace(1, 0, w))[None, :]
    y = np.minimum(np.linspace(0, 1, h), np.linspace(1, 0, h))[:, None]
    al = np.clip(x * 12, 0, 1) * np.clip(y * 5, 0, 1) * strength
    img = np.zeros((h, w, 4), np.float32)
    img[..., 0], img[..., 1], img[..., 2] = 255, 120, 24
    img[..., 3] = al * 255
    return Image.fromarray(img.round().astype(np.uint8), 'RGBA')


def plate(w, h, radius, fill, border):
    """rounded dark plate with a thin orange border, drawn 4x and reduced"""
    from PIL import ImageDraw
    k = 4
    img = Image.new('RGBA', (w * k, h * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w * k - 1, h * k - 1], radius * k, fill=border)
    d.rounded_rectangle([k, k, w * k - 1 - k, h * k - 1 - k], (radius - 1) * k, fill=fill)
    return resize(img, w, h)


if __name__ == '__main__':
    main()
