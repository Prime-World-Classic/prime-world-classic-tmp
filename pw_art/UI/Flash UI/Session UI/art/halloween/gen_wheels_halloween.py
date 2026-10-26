"""Halloween textures of the chat and ping wheels (the same geometry as pw_art/UI/PingWheel/wheel_gen.py).

  python gen_wheels_halloween.py

Writes to pw/branches/r1117/Data/UI/Styles/Icons/ChatWheel/chat_wheel_hw_bg_<N>.dds, chat_wheel_hw_hl_<N>_<i>.dds
and .../PingWheel/ping_wheel_hw_*.dds (+ previews into previews/). The highlight sectors keep the shape of the usual
ones, so CHAT_HL_OFFSETS / PING_HL_OFFSETS of MainInterface.as stay valid (checked here).
Disc: the wood of the windows (generated/wood_back.png), orange glowing rim, sector lines and centre.
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops, ImageEnhance

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..', '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'pw_art', 'UI', 'PingWheel'))
from wheel_gen import wheel, as_offsets, radial, pow2, sector_angle, COUNTS, SS

DATA = os.path.join(ROOT, 'pw', 'branches', 'r1117', 'Data', 'UI', 'Styles', 'Icons')
PREVIEWS = os.path.join(HERE, 'previews')

ORANGE = (255, 128, 24)
EMBER = (255, 170, 70)
DARK = (22, 9, 18)


def wood_disc(S):
    wood = Image.open(os.path.join(HERE, 'generated', 'wood_back.png')).convert('RGB')
    wood = ImageEnhance.Brightness(wood).enhance(0.55).resize((S, S), Image.LANCZOS)
    return wood.convert('RGBA')


def save_dds(img, path):
    # uncompressed A8R8G8B8 (as the ping icons): DXT5 blurs thin lines on the wood into 4x4 blocks
    img.convert('RGBA').save(path, format='DDS')


def fade_mask(size, r0, r1, edge=0.14):
    """L mask: 255 between r0 and r1 with smooth fades of 'edge' of the length at both ends, 0 elsewhere."""
    m = Image.new('L', (size, size), 0)
    px = m.load()
    cx = cy = size / 2.0
    for y in range(size):
        for x in range(size):
            t = (math.hypot(x + 0.5 - cx, y + 0.5 - cy) - r0) / (r1 - r0)
            if t <= 0 or t >= 1:
                continue
            e = min(t, 1 - t) / edge
            v = 1.0 if e >= 1 else e * e * (3 - 2 * e)
            px[x, y] = int(255 * v)
    return m


def wheel_halloween(out, prefix, size, r_out, r_in, slot_r):
    os.makedirs(out, exist_ok=True)
    os.makedirs(PREVIEWS, exist_ok=True)
    S = size * SS
    c = S / 2.0
    k = size / 256.0

    def circle(r):
        return [c - r * SS, c - r * SS, c + r * SS, c + r * SS]

    wood = wood_disc(S)
    # darker to the rim
    shade = radial(size, (size / 2, size / 2), r_in, r_out, (0, 0, 0, 40), (0, 0, 0, 150)).resize((S, S), Image.BICUBIC)
    wood = Image.alpha_composite(wood, shade)
    grad_src = radial(size, (size / 2, size / 2), r_in, r_out, ORANGE + (40,), ORANGE + (120,)).resize((S, S), Image.BICUBIC)
    ref_grad = radial(size, (size / 2, size / 2), r_in, r_out, (110, 220, 205, 30), (140, 235, 220, 100)).resize((S, S), Image.BICUBIC)
    line_fade = fade_mask(size, r_in + 5 * k, r_out - 7 * k).resize((S, S), Image.BICUBIC)
    offsets = {}
    for n in COUNTS:
        bg = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        sh = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(sh).ellipse(circle(r_out + 2 * k), fill=(0, 0, 0, 110))
        sh = sh.filter(ImageFilter.GaussianBlur(5 * k * SS))
        bg = Image.alpha_composite(bg, sh)
        disc = wood.copy()
        mask = Image.new('L', (S, S), 0)
        ImageDraw.Draw(mask).ellipse(circle(r_out), fill=232)
        disc.putalpha(mask)
        bg = Image.alpha_composite(bg, disc)
        # glow of the rim inside the disc
        glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(glow).ellipse(circle(r_out - 4 * k), outline=ORANGE + (200,), width=int(4 * k * SS))
        glow = glow.filter(ImageFilter.GaussianBlur(4 * k * SS))
        glow.putalpha(ImageChops.multiply(glow.getchannel('A'), mask))
        bg = Image.alpha_composite(bg, glow)
        d = ImageDraw.Draw(bg)
        d.ellipse(circle(r_out), outline=DARK + (255,), width=int(3 * k * SS))
        d.ellipse(circle(r_out - 2.5 * k), outline=ORANGE + (255,), width=int(1.5 * k * SS))
        d.ellipse(circle(r_out - 4.5 * k), outline=(60, 20, 10, 220), width=max(SS, int(k * SS)))
        # sector dividers: solid lines (dark groove + ember core), faded only at the ends by one smooth mask
        lines = Image.new('RGBA', (S, S), (0, 0, 0, 0))
        ld = ImageDraw.Draw(lines)
        t0, t1 = r_in + 5 * k, r_out - 7 * k
        for i in range(n):
            a = math.radians(sector_angle(i, n) + 180.0 / n)
            p0 = (c + math.cos(a) * t0 * SS, c + math.sin(a) * t0 * SS)
            p1 = (c + math.cos(a) * t1 * SS, c + math.sin(a) * t1 * SS)
            ld.line([p0, p1], fill=(14, 4, 8, 235), width=int(3.4 * k * SS))
        for i in range(n):
            a = math.radians(sector_angle(i, n) + 180.0 / n)
            p0 = (c + math.cos(a) * t0 * SS, c + math.sin(a) * t0 * SS)
            p1 = (c + math.cos(a) * t1 * SS, c + math.sin(a) * t1 * SS)
            ld.line([p0, p1], fill=EMBER + (255,), width=int(1.3 * k * SS))
        lines.putalpha(ImageChops.multiply(lines.getchannel('A'), line_fade))
        bg = Image.alpha_composite(bg, lines)
        d = ImageDraw.Draw(bg)
        d.ellipse(circle(r_in), fill=DARK + (245,), outline=ORANGE + (255,), width=int(1.5 * k * SS))
        if slot_r > 0:
            r_slot = (r_in + r_out) / 2.0 + 2
            for i in range(n):
                a = math.radians(sector_angle(i, n))
                sx, sy = c + math.cos(a) * r_slot * SS, c + math.sin(a) * r_slot * SS
                d.ellipse([sx - slot_r * SS, sy - slot_r * SS, sx + slot_r * SS, sy + slot_r * SS],
                          fill=DARK + (245,), outline=ORANGE + (255,), width=int(1.5 * SS))
        bg = bg.resize((size, size), Image.LANCZOS)
        save_dds(bg, os.path.join(out, "%s_bg_%d.dds" % (prefix, n)))

        offsets[n] = []
        for i in range(n):
            # the same shape as wheel_gen.py: the crop and its offset must not change
            hl = Image.new('RGBA', (S, S), (0, 0, 0, 0))
            a0 = sector_angle(i, n) - 180.0 / n + 1.2
            a1 = sector_angle(i, n) + 180.0 / n - 1.2
            m = Image.new('L', (S, S), 0)
            ImageDraw.Draw(m).pieslice(circle(r_out - 3 * k), a0, a1, fill=255)
            ImageDraw.Draw(m).ellipse(circle(r_in + 3 * k), fill=0)
            g = grad_src.copy()
            g.putalpha(ImageChops.multiply(g.getchannel('A'), m))
            hl = Image.alpha_composite(hl, g)
            ImageDraw.Draw(hl).arc(circle(r_out - 3 * k), a0, a1, fill=(255, 200, 110, 240), width=int(3 * k * SS))
            hl = hl.resize((size, size), Image.LANCZOS)
            # crop box of the usual highlight (its alpha: wheel_gen.py), so the offsets stay the same
            ref = Image.new('RGBA', (S, S), (0, 0, 0, 0))
            rg = ref_grad.copy()
            rg.putalpha(ImageChops.multiply(rg.getchannel('A'), m))
            ref = Image.alpha_composite(ref, rg)
            ImageDraw.Draw(ref).arc(circle(r_out - 3 * k), a0, a1, fill=(236, 232, 180, 235), width=int(3 * k * SS))
            ref = ref.resize((size, size), Image.LANCZOS)
            x0, y0, x1, y1 = ref.getchannel('A').getbbox()
            hw = pow2(x1 - x0)
            hh = pow2(y1 - y0)
            ox = min(max(int((x0 + x1) / 2 - hw / 2), 0), size - hw)
            oy = min(max(int((y0 + y1) / 2 - hh / 2), 0), size - hh)
            crop = hl.crop((ox, oy, ox + hw, oy + hh))
            save_dds(crop, os.path.join(out, "%s_hl_%d_%d.dds" % (prefix, n, i)))
            offsets[n].append((ox - size // 2, oy - size // 2))
            if i == 1:
                p = bg.copy()
                p.alpha_composite(crop, (ox, oy))
                p.save(os.path.join(PREVIEWS, '%s_preview_%d.png' % (prefix, n)))
    return offsets


def check(name, hw, classic):
    if hw != classic:
        raise SystemExit('%s: highlight offsets differ from the usual wheel, update MainInterface.as' % name)
    print(name, 'ok')


if __name__ == '__main__':
    raise SystemExit('the wheels are 2x textures now: python pw_art/UI/PingWheel/wheel_hd.py (both skins)')
    import tempfile
    tmp = tempfile.mkdtemp()
    chat = wheel_halloween(os.path.join(DATA, 'ChatWheel'), 'chat_wheel_hw', 512, 244, 58, 0)
    check('chat', chat, wheel(tmp, 'chat_wheel', 512, 244, 58, 0, preview=False))
    ping = wheel_halloween(os.path.join(DATA, 'PingWheel'), 'ping_wheel_hw', 256, 124, 42, 26)
    check('ping', ping, wheel(tmp, 'ping_wheel', 256, 124, 42, 26, preview=False))
    # the chat wheel of the options window (160 px): the 512 texture shrunk 3x without mipmaps has jagged edges,
    # so the preview has its own 256 textures (half radii); their offsets go to OptionsWindow.CHAT_HL_OFFSETS_SMALL
    small = wheel_halloween(os.path.join(DATA, 'ChatWheel'), 'chat_wheel_hw_s', 256, 122, 29, 0)
    print(as_offsets('CHAT_HL_OFFSETS_SMALL', small))
