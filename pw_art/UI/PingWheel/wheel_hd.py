"""High-resolution textures of the chat and ping wheels (classic and halloween skins).

  python wheel_hd.py

Writes into pw/branches/r1117/Data/UI/Styles/Icons/ChatWheel and .../PingWheel (previews into previews_hd/ next to this file):
  chat_wheel[_hw]_bg_<N>.dds, chat_wheel[_hw]_hl_<N>_<i>.dds      2x textures of the 512 px chat wheel (1024 px)
  ping_wheel[_hw]_bg_<N>.dds, ping_wheel[_hw]_hl_<N>_<i>.dds      2x textures of the 256 px ping wheel (512 px)
  chat_wheel[_hw]_s_*, ping_wheel[_hw]_s_*                         1x textures of the wheel previews of the options window
and prints the highlight offsets for MainInterface.as / OptionsWindow.as (in wheel pixels, i.e. texture pixels / scale).
The 2x textures are shown with scale 1/2 (MainInterface.WHEEL_TEX_SCALE).

Everything is computed per pixel in float (analytic anti-aliasing) and saved uncompressed A8R8G8B8 with a light dither:
DXT5 turned the radial gradient of the disc into steps. Sector dividers are tapered strokes: full width in the middle,
narrowing smoothly to a point at both ends.
"""
import math, os
import numpy as np
from PIL import Image
from scipy.special import erfc

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
ICONS = os.path.join(ROOT, 'pw', 'branches', 'r1117', 'Data', 'UI', 'Styles', 'Icons')
WOOD = os.path.join(ROOT, 'pw_art', 'UI', 'Flash UI', 'Session UI', 'art', 'halloween', 'generated', 'wood_back.png')
PREVIEWS = os.path.join(HERE, 'previews_hd')

COUNTS = range(4, 9)


def pow2(v):
    p = 16
    while p < v:
        p *= 2
    return p


def sector_angle(i, n):
    return -90 + i * 360.0 / n


# ------------------------------------------------------------------ float RGBA canvas (premultiplied)
class Canvas(object):
    def __init__(self, size):
        self.size = size
        self.rgb = np.zeros((size, size, 3))   # premultiplied
        self.a = np.zeros((size, size))

    def over(self, color, alpha):
        """color: (r, g, b) 0..255 or an HxWx3 array; alpha: HxW 0..1"""
        col = np.asarray(color, dtype=float) / 255.0
        a = np.clip(alpha, 0, 1)
        self.rgb = col * a[..., None] + self.rgb * (1 - a[..., None])
        self.a = a + self.a * (1 - a)

    def image(self, dither=True):
        a = self.a
        rgb = np.where(a[..., None] > 1e-6, self.rgb / np.maximum(a[..., None], 1e-6), 0)
        out = np.dstack([rgb, a]) * 255.0
        if dither:
            rng = np.random.RandomState(1)
            out += (rng.random_sample(out.shape) - rng.random_sample(out.shape)) * 0.5
        return Image.fromarray(np.clip(np.round(out), 0, 255).astype(np.uint8), 'RGBA')


def save_dds(img, path):
    img.save(path, format='DDS')  # uncompressed A8R8G8B8


# ------------------------------------------------------------------ geometry (all radii in wheel pixels)
class Geo(object):
    def __init__(self, size, scale):
        self.size = size              # wheel pixels
        self.scale = scale            # texture pixels per wheel pixel
        self.tex = int(size * scale)
        c = self.tex / 2.0
        y, x = np.mgrid[0:self.tex, 0:self.tex]
        self.x = (x + 0.5 - c) / scale    # wheel pixels from the center
        self.y = (y + 0.5 - c) / scale
        self.r = np.hypot(self.x, self.y)
        self.ang = np.degrees(np.arctan2(self.y, self.x))   # screen angles (y down), like PIL
        self.px = 1.0 / scale         # one texture pixel in wheel pixels

    def disc(self, R, x0=0.0, y0=0.0):
        r = self.r if (x0 == 0 and y0 == 0) else np.hypot(self.x - x0, self.y - y0)
        return np.clip((R - r) / self.px + 0.5, 0, 1)

    def band(self, r0, r1, x0=0.0, y0=0.0):
        """coverage of r0 <= r <= r1"""
        return np.clip(self.disc(r1, x0, y0) - self.disc(r0, x0, y0), 0, 1)

    def ring(self, R, width):
        """PIL ellipse outline: width drawn inside the radius"""
        return self.band(R - width, R)

    def radial(self, r0, r1, c0, c1):
        t = np.clip((self.r - r0) / (r1 - r0), 0, 1)[..., None]
        c0 = np.asarray(c0, dtype=float)
        c1 = np.asarray(c1, dtype=float)
        col = c0 + (c1 - c0) * t
        return col[..., :3], col[..., 3] / 255.0

    def shadow(self, R, sigma, alpha):
        return alpha / 255.0 * 0.5 * erfc((self.r - R) / (sigma * math.sqrt(2)))

    def sector(self, a0, a1):
        """angular coverage between a0..a1 degrees (anti-aliased by the distance to the edge lines)"""
        mid = (a0 + a1) / 2.0
        half = (a1 - a0) / 2.0
        d = (self.ang - mid + 180.0) % 360.0 - 180.0
        inside = np.radians(half - np.abs(d))
        dist = np.where(np.abs(inside) < math.pi / 2, self.r * np.sin(inside), np.sign(inside) * self.r)
        return np.clip(dist / self.px + 0.5, 0, 1)

    def divider(self, angle, t0, t1, width, power=0.55):
        """tapered stroke along the ray 'angle' from radius t0 to t1: width in the middle, 0 at the ends.
        Returns the coverage 0..1."""
        a = math.radians(angle)
        ca, sa = math.cos(a), math.sin(a)
        along = self.x * ca + self.y * sa
        perp = np.abs(-self.x * sa + self.y * ca)
        u = np.clip((along - t0) / (t1 - t0), 0, 1)
        half = 0.5 * width * np.sin(math.pi * u) ** power
        half = np.where((along > t0) & (along < t1), half, 0)
        # thin parts get thinner coverage instead of disappearing into aliasing
        cov = np.clip((half - perp) / self.px + 0.5, 0, 1)
        thin = np.clip(2 * half / self.px, 0, 1)
        return np.maximum(cov * np.where(2 * half < self.px, thin, 1), 0) * (half > 0)


# ------------------------------------------------------------------ styles
CLASSIC = dict(
    shadow=90,
    disc=((33, 74, 72, 214), (14, 40, 40, 238)),
    rim=((70, 56, 24), 1.0), rim2=((168, 170, 127), 1.0), rim3=((67, 67, 29), 220 / 255.0),
    line=None,
    center=((8, 34, 32), 240 / 255.0), outline=(168, 170, 127),
    slot=((14, 46, 44), 245 / 255.0),
    hl=((110, 220, 205, 30), (140, 235, 220, 100)), hl_arc=((236, 232, 180), 235 / 255.0),
)
ORANGE = (255, 128, 24)
EMBER = (255, 170, 70)
DARK = (22, 9, 18)
HALLOWEEN = dict(
    shadow=110,
    disc=None,
    rim=(DARK, 1.0), rim2=(ORANGE, 1.0), rim3=((60, 20, 10), 220 / 255.0),
    center=(DARK, 245 / 255.0), outline=ORANGE,
    slot=(DARK, 245 / 255.0),
    hl=(ORANGE + (40,), ORANGE + (120,)), hl_arc=((255, 200, 110), 240 / 255.0),
)


def wood_rgb(tex):
    img = Image.open(WOOD).convert('RGB').resize((tex, tex), Image.LANCZOS)
    return np.asarray(img, dtype=float) * 0.55


def wheel(out, prefix, size, r_out, r_in, slot_r, scale, style):
    """writes the textures; returns {N: [(x, y), ...]} - highlight offsets in wheel pixels from the center"""
    os.makedirs(out, exist_ok=True)
    os.makedirs(PREVIEWS, exist_ok=True)
    g = Geo(size, scale)
    k = size / 256.0
    hw = style is HALLOWEEN
    offsets = {}
    for n in COUNTS:
        cv = Canvas(g.tex)
        cv.over((0, 0, 0), g.shadow(r_out + 2 * k, 5 * k, style['shadow']))
        mask = g.disc(r_out)
        if hw:
            wood = wood_rgb(g.tex)
            shade_col, shade_a = g.radial(r_in, r_out, (0, 0, 0, 40), (0, 0, 0, 150))
            wood = wood * (1 - shade_a[..., None])
            cv.over(wood, mask * 232 / 255.0)
            # glow of the rim inside the disc
            glow = np.exp(-((g.r - (r_out - 6 * k)) ** 2) / (2 * (4.5 * k) ** 2)) * 200 / 255.0
            cv.over(ORANGE, glow * mask)
        else:
            col, a = g.radial(r_in, r_out, *style['disc'])
            cv.over(col, a * mask)
        cv.over(style['rim'][0], g.ring(r_out, 3 * k) * style['rim'][1])
        cv.over(style['rim2'][0], g.ring(r_out - 2.5 * k, 1.5 * k) * style['rim2'][1])
        cv.over(style['rim3'][0], g.ring(r_out - 4.5 * k, max(k, 0.75)) * style['rim3'][1])
        for i in range(n):
            ang = sector_angle(i, n) + 180.0 / n
            if hw:
                t0, t1 = r_in + 5 * k, r_out - 7 * k
                cv.over((14, 4, 8), g.divider(ang, t0, t1, 3.4 * k) * 235 / 255.0)
                cv.over(EMBER, g.divider(ang, t0 + 2 * k, t1 - 2 * k, 1.4 * k))
            else:
                t0, t1 = r_in + 6 * k, r_out - 9 * k
                cv.over((150, 190, 175), g.divider(ang, t0, t1, 2.0 * k) * 175 / 255.0)
        cv.over(style['center'][0], g.disc(r_in) * style['center'][1])
        cv.over(style['outline'], g.ring(r_in, 1.5 * k))
        if slot_r > 0:
            r_slot = (r_in + r_out) / 2.0 + 2
            for i in range(n):
                a = math.radians(sector_angle(i, n))
                sx, sy = math.cos(a) * r_slot, math.sin(a) * r_slot
                cv.over(style['slot'][0], g.disc(slot_r, sx, sy) * style['slot'][1])
                cv.over(style['outline'], g.band(slot_r - 1.5, slot_r, sx, sy))
        bg = cv.image()
        save_dds(bg, os.path.join(out, '%s_bg_%d.dds' % (prefix, n)))

        offsets[n] = []
        for i in range(n):
            a0 = sector_angle(i, n) - 180.0 / n + 1.2
            a1 = sector_angle(i, n) + 180.0 / n - 1.2
            shape = g.sector(a0, a1) * g.band(r_in + 3 * k, r_out - 3 * k)
            hc = Canvas(g.tex)
            col, a = g.radial(r_in, r_out, *style['hl'])
            hc.over(col, a * shape)
            hc.over(style['hl_arc'][0], g.ring(r_out - 3 * k, 3 * k) * g.sector(a0, a1) * style['hl_arc'][1])
            hl = hc.image(dither=False)
            # the crop: the smallest power-of-two texture around the sector, its corner on whole wheel pixels
            ys, xs = np.nonzero(shape > 0.004)
            x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
            tw = pow2(x1 - x0)
            th = pow2(y1 - y0)
            step = int(scale)
            ox = min(max(int((x0 + x1) / 2 - tw / 2) // step * step, 0), g.tex - tw)
            oy = min(max(int((y0 + y1) / 2 - th / 2) // step * step, 0), g.tex - th)
            crop = hl.crop((ox, oy, ox + tw, oy + th))
            save_dds(crop, os.path.join(out, '%s_hl_%d_%d.dds' % (prefix, n, i)))
            offsets[n].append((int(ox / scale) - size // 2, int(oy / scale) - size // 2))
            if i == 1:
                p = bg.copy()
                p.alpha_composite(crop, (ox, oy))
                p.save(os.path.join(PREVIEWS, '%s_preview_%d.png' % (prefix, n)))
    return offsets


def as_offsets(name, offsets):
    rows = []
    for n in COUNTS:
        rows.append('[' + ','.join('%d,%d' % o for o in offsets[n]) + ']')
    return '%s = [null,null,null,null,%s];' % (name, ','.join(rows))


if __name__ == '__main__':
    chat_dir = os.path.join(ICONS, 'ChatWheel')
    ping_dir = os.path.join(ICONS, 'PingWheel')
    result = {}
    for style, suffix in ((CLASSIC, ''), (HALLOWEEN, 'hw_')):
        result[('chat', suffix)] = wheel(chat_dir, 'chat_wheel_' + suffix.rstrip('_') if suffix else 'chat_wheel', 512, 244, 58, 0, 2, style)
        result[('ping', suffix)] = wheel(ping_dir, 'ping_wheel_' + suffix.rstrip('_') if suffix else 'ping_wheel', 256, 124, 42, 26, 2, style)
        # previews of the options window (160 px): 1x textures, the chat one with half radii
        result[('chat_s', suffix)] = wheel(chat_dir, 'chat_wheel_' + suffix + 's', 256, 122, 29, 0, 1, style)
        result[('ping_s', suffix)] = wheel(ping_dir, 'ping_wheel_' + suffix + 's', 256, 124, 42, 26, 1, style)
    for key in ('chat', 'ping', 'chat_s', 'ping_s'):
        if result[(key, '')] != result[(key, 'hw_')]:
            raise SystemExit('%s: classic and halloween offsets differ' % key)
    print(as_offsets('CHAT_HL_OFFSETS', result[('chat', '')]))
    print(as_offsets('PING_HL_OFFSETS', result[('ping', '')]))
    print(as_offsets('CHAT_HL_OFFSETS_SMALL', result[('chat_s', '')]))
    print(as_offsets('PING_HL_OFFSETS_SMALL', result[('ping_s', '')]))
