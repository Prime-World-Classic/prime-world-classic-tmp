"""Laguna: light-red skill effects for the enemies (allies keep the original blue ones; the switching is done by the
existing *_Team.EFFT EffectSwitchers).

usage: py -3 gen_laguna_red_fx.py            (from anywhere; paths are taken from the repository)
       py -3 gen_laguna_red_fx.py --check    (only report what would change, write nothing)
       py -3 gen_laguna_red_fx.py --rebuild-keys   (rebuild laguna_red_part_keys.json from git history)

Every existing *_Red file of Heroes/Laguna and Summons/Laguna is rebuilt from its ORIGINAL (blue) file; the names, GUIDs,
hrefs and BackLinks of the *_Red files are not touched, only colour values are written:
  *_Red.PRXY.xdb / *_Red.ANSC.xdb  the existing red file is the template (ids, links); every <B><G><R> colour block takes
                                   the recoloured value of the same block of the original (neutral / grey ones stay)
  *_Red.part                       the original bytes with the colour keys (float R,G,B) recoloured; the file is padded with
                                   zeros to a multiple of 1024 bytes, as the existing red files are. The key positions are in
                                   laguna_red_part_keys.json (stride 4 floats, runs), taken from the first version of the
                                   red files (commit b03afd618): the places where they differed from the originals
  *_Red.dds                        DXT5. Same layout as the original (DXT5, same mip count): only the two end colours of
                                   every 4x4 block are recoloured (alpha, indices and mips stay exactly). Otherwise (the
                                   original is DXT1 / has no mips) the levels are decoded, recoloured and encoded again
  Heroes/Laguna/EffectsRed/Tex/*   sources are the shared GFX_Textures (atlas_0, Wave09Blue, Wave11Blue)
Running it again gives the same files.

Colour rule (recolour()): blue / cyan / green / violet hues (70..340 degrees, smooth edges of 20 degrees) with saturation above ~0.1
turn into a light red, hue 357..4 degrees (210 -> 1), the brightness stays and the saturation is scaled by 0.85; greys, whites, reds, pinks stay.
"""
import json, os, re, struct, subprocess, sys
import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
DATA = os.path.join(REPO, 'pw', 'branches', 'r1117', 'Data')
ROOTS = ['Heroes/Laguna', 'Summons/Laguna']
KEYS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'laguna_red_part_keys.json')
FIRST_RED_COMMIT = 'b03afd618'

# red textures whose originals are shared game textures
TEX_SOURCES = {
    'Heroes/Laguna/EffectsRed/Tex/atlas_0_Red.dds': 'GFX_Textures/Atlases/atlas_0.dds',
    'Heroes/Laguna/EffectsRed/Tex/Wave09Blue_Red.dds': 'GFX_Textures/Wave09Blue.dds',
    'Heroes/Laguna/EffectsRed/Tex/Wave11Blue_Red.dds': 'GFX_Textures/Wave11Blue.dds',
}

HUE_BASE = 1.0      # result hue for a blue of 210 degrees
SAT_SCALE = 0.85    # a little paler than the blue, so that it reads as a light red, not a deep one
HUE_SPREAD = 0.03   # the source hue range 90..320 (full weight) is squeezed into about 357..4 degrees


# ---------------------------------------------------------------------------------------------------- colour

def recolour(rgb):
    """rgb: float array (..., 3) in R, G, B order (values may exceed 1); returns the recoloured array"""
    rgb = np.asarray(rgb, dtype=np.float64)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = rgb.max(-1)
    mn = rgb.min(-1)
    d = mx - mn
    safe = np.where(d > 0, d, 1.0)
    h = np.where(mx == r, ((g - b) / safe) % 6.0, np.where(mx == g, (b - r) / safe + 2.0, (r - g) / safe + 4.0)) * 60.0
    s = np.where(mx > 0, d / np.where(mx > 0, mx, 1.0), 0.0)
    w = np.clip((h - 70.0) / 20.0, 0, 1) * np.clip((340.0 - h) / 20.0, 0, 1) * np.clip((s - 0.09) / 0.04, 0, 1)
    nh = ((HUE_BASE + (h - 210.0) * HUE_SPREAD) % 360.0) / 60.0
    # hsv -> rgb with the saturation s * SAT_SCALE and the same v (= mx)
    c = mx * s * SAT_SCALE
    x = c * (1.0 - np.abs(nh % 2.0 - 1.0))
    m = mx - c
    sector = np.floor(nh).astype(int) % 6
    zero = np.zeros_like(c)
    rr = np.select([sector == 0, sector == 1, sector == 2, sector == 3, sector == 4, sector == 5], [c, x, zero, zero, x, c])
    gg = np.select([sector == 0, sector == 1, sector == 2, sector == 3, sector == 4, sector == 5], [x, c, c, x, zero, zero])
    bb = np.select([sector == 0, sector == 1, sector == 2, sector == 3, sector == 4, sector == 5], [zero, zero, x, c, c, x])
    rot = np.stack([rr + m, gg + m, bb + m], -1)
    return rgb + w[..., None] * (rot - rgb)


def hsv(rgb):
    import colorsys
    h, s, v = colorsys.rgb_to_hsv(*[float(x) for x in rgb])
    return round(h * 360), round(s, 2), round(v, 2)


# ---------------------------------------------------------------------------------------------------- files

def red_files(ext):
    out = []
    for root in ROOTS:
        for dp, dn, fn in os.walk(os.path.join(DATA, *root.split('/'))):
            for f in fn:
                if '_Red' in f and f.endswith(ext):
                    out.append(os.path.join(dp, f))
    return sorted(out)


def rel(p):
    return os.path.relpath(p, DATA).replace('\\', '/')


def original_of(p):
    r = rel(p)
    if r in TEX_SOURCES:
        return os.path.join(DATA, *TEX_SOURCES[r].split('/'))
    return p.replace('_Red', '')


def write_if_changed(p, data, stats, check):
    old = open(p, 'rb').read() if os.path.exists(p) else None
    if old == data:
        stats['same'] += 1
        return
    stats['changed'].append(rel(p))
    if not check:
        with open(p, 'wb') as f:
            f.write(data)


# ---------------------------------------------------------------------------------------------------- .part

def build_keys():
    """runs of colour keys of every red .part: {rel path: [[first float index, count], ...]} (stride 4 floats)"""
    keys = {}
    for p in red_files('.part'):
        r = rel(p)
        a = open(original_of(p), 'rb').read()
        b = subprocess.check_output(['git', '-C', REPO, 'show', '%s:pw/branches/r1117/Data/%s' % (FIRST_RED_COMMIT, r)])
        b = b[:len(a)]
        n = len(a) // 4
        fa = np.frombuffer(a[:n * 4], dtype=np.uint32)
        fb = np.frombuffer(b[:n * 4], dtype=np.uint32)
        diff = np.nonzero(fa != fb)[0]
        if not len(diff):
            continue
        assert len(diff) % 3 == 0 and np.all(diff[0::3] + 2 == diff[2::3]) and np.all(diff[0::3] + 1 == diff[1::3]), r
        starts = diff[0::3].tolist()
        runs = []
        for s in starts:
            if runs and s - runs[-1][0] - 4 * (runs[-1][1] - 1) == 4:
                runs[-1][1] += 1
            else:
                runs.append([s, 1])
        keys[r] = runs
    with open(KEYS_FILE, 'w') as f:
        json.dump(keys, f, separators=(',', ':'), sort_keys=True)
    return keys


def load_keys():
    if not os.path.exists(KEYS_FILE):
        return build_keys()
    return json.load(open(KEYS_FILE))


def make_part(p, keys, stats, check, hues):
    a = open(original_of(p), 'rb').read()
    n = len(a) // 4
    arr = np.frombuffer(a[:n * 4], dtype='<f4').copy()
    idx = []
    for s, c in keys.get(rel(p), []):
        idx.append(s + 4 * np.arange(c))
    if idx:
        idx = np.concatenate(idx)
        cols = np.stack([arr[idx], arr[idx + 1], arr[idx + 2]], -1)
        new = recolour(cols).astype('<f4')
        for k in range(3):
            arr[idx + k] = new[:, k]
        hues.append((rel(p), cols, new))
    data = arr.tobytes() + a[n * 4:]
    data += b'\0' * (-len(data) % 1024)
    write_if_changed(p, data, stats, check)


# ---------------------------------------------------------------------------------------------------- .xdb

COLOR = re.compile(r'(<B>)([^<]*)(</B>\s*<G>)([^<]*)(</G>\s*<R>)([^<]*)(</R>)')


def fmt(x):
    s = ('%.6g' % x)
    return s


def make_xdb(p, stats, check, hues, problems):
    o = original_of(p)
    if not os.path.exists(o):
        return
    src = open(o, 'rb').read().decode('utf-8-sig')
    dst = open(p, 'rb').read().decode('utf-8-sig')
    cs = COLOR.findall(src)
    cd = COLOR.findall(dst)
    if len(cs) != len(cd):
        problems.append('%s: %d colour blocks in the original, %d in the red file' % (rel(p), len(cs), len(cd)))
        return
    if not cs:
        return
    cols = np.array([[float(c[5]), float(c[3]), float(c[1])] for c in cs])
    new = recolour(cols)
    k = [0]

    def sub(m):
        i = k[0]
        k[0] += 1
        if np.allclose(new[i], cols[i], atol=1e-9):
            # not recoloured: stays as the red file has it (equal to the original)
            return m.group(0)
        return (m.group(1) + fmt(new[i][2]) + m.group(3) + fmt(new[i][1]) + m.group(5) + fmt(new[i][0]) + m.group(7))
    out = COLOR.sub(sub, dst)
    ch = [i for i in range(len(cs)) if not np.allclose(new[i], cols[i], atol=1e-9)]
    if ch:
        hues.append((rel(p), cols[ch], new[ch]))
    write_if_changed(p, b'\xef\xbb\xbf' + out.encode('utf-8'), stats, check)


# ---------------------------------------------------------------------------------------------------- .dds

def dds_levels(b):
    h, w = struct.unpack_from('<II', b, 12)
    mips = struct.unpack_from('<I', b, 28)[0] or 1
    fmt_ = b[84:88]
    assert fmt_ in (b'DXT1', b'DXT5'), fmt_
    bs = 8 if fmt_ == b'DXT1' else 16
    lv, off = [], 128
    for i in range(mips):
        lw, lh = max(1, w >> i), max(1, h >> i)
        size = max(1, (lw + 3) // 4) * max(1, (lh + 3) // 4) * bs
        lv.append((lw, lh, off, size))
        off += size
    return fmt_, lv, off


def decode_level(b, fmt_, lw, lh, off, size):
    import texture2ddecoder
    pw_, ph_ = max(4, (lw + 3) // 4 * 4), max(4, (lh + 3) // 4 * 4)
    raw = bytes(b[off:off + size])
    px = texture2ddecoder.decode_bc3(raw, pw_, ph_) if fmt_ == b'DXT5' else texture2ddecoder.decode_bc1(raw, pw_, ph_)
    a = np.frombuffer(px, dtype=np.uint8).reshape(ph_, pw_, 4)[:lh, :lw]
    return a[..., [2, 1, 0, 3]].astype(np.float32) / 255.0


def encode_level(img):
    import etcpak
    h, w = img.shape[:2]
    ph, pw_ = max(4, (h + 3) // 4 * 4), max(4, (w + 3) // 4 * 4)
    if (ph, pw_) != (h, w):
        img = np.pad(img, ((0, ph - h), (0, pw_ - w), (0, 0)), mode='edge')
    rgba = np.clip(np.rint(img * 255.0), 0, 255).astype(np.uint8)
    return bytes(etcpak.compress_bc3(rgba.tobytes(), pw_, ph))


def rgb565(c):
    return np.stack([((c >> 11) & 31) / 31.0, ((c >> 5) & 63) / 63.0, (c & 31) / 31.0], -1)


def to565(rgb):
    q = lambda x, m: np.clip(np.rint(x * m), 0, m).astype(np.uint32)
    return (q(rgb[..., 0], 31) << 11) | (q(rgb[..., 1], 63) << 5) | q(rgb[..., 2], 31)


def make_dds(p, stats, check, info):
    o = original_of(p)
    red = open(p, 'rb').read()
    src = open(o, 'rb').read()
    rf, rlv, rend = dds_levels(red)
    sf, slv, send = dds_levels(src)
    assert rf == b'DXT5' and rend == len(red), rel(p)
    assert struct.unpack_from('<II', red, 12) == struct.unpack_from('<II', src, 12), rel(p)
    if sf == b'DXT5' and len(src) == len(red) and len(slv) == len(rlv):
        # same layout: only the end colours of every block
        out = bytearray(src)
        out[:128] = red[:128]  # the header of the red file, without the tool signature in the reserved bytes
        out[32:76] = bytes(44)
        for lw, lh, off, size in slv:
            blocks = np.frombuffer(bytes(out[off:off + size]), dtype=np.uint8).reshape(-1, 16).copy()
            c = blocks[:, 8:12].copy().view('<u2').reshape(-1, 2)
            cols = np.stack([rgb565(c[:, 0]), rgb565(c[:, 1])], 1)
            new = recolour(cols)
            n565 = np.stack([to565(new[:, 0]), to565(new[:, 1])], 1).astype('<u2')
            blocks[:, 8:12] = n565.view(np.uint8).reshape(-1, 4)
            out[off:off + size] = blocks.tobytes()
        mode = 'endpoints'
    else:
        # decode, recolour, encode again (the mip chain of the original if it has one, else a box filter)
        lv0 = decode_level(src, sf, *slv[0])
        levels = []
        if len(slv) == len(rlv):
            levels = [decode_level(src, sf, *l) for l in slv]
        else:
            levels = [lv0]
            cur = lv0
            for i in range(1, len(rlv)):
                h, w = cur.shape[:2]
                if h > 1 and w > 1:
                    cur = cur[:h // 2 * 2, :w // 2 * 2].reshape(h // 2, 2, w // 2, 2, 4).mean((1, 3))
                elif w > 1:
                    cur = cur[:, :w // 2 * 2].reshape(h, w // 2, 2, 4).mean(2)
                else:
                    cur = cur[:h // 2 * 2].reshape(h // 2, 2, w, 4).mean(1)
                levels.append(cur)
        out = bytearray(red[:128])
        for lw, lh, off, size in rlv:
            img = levels[rlv.index((lw, lh, off, size))]
            rgb = recolour(img[..., :3])
            blk = encode_level(np.concatenate([rgb, img[..., 3:]], -1).astype(np.float32))
            assert len(blk) == size, (rel(p), len(blk), size)
            out += blk
        mode = 'reencode (%s -> DXT5, %d -> %d mips)' % (sf.decode(), len(slv), len(rlv))
    assert len(out) == len(red), rel(p)
    info.append((rel(p), mode))
    write_if_changed(p, bytes(out), stats, check)


# ---------------------------------------------------------------------------------------------------- main

def main():
    check = '--check' in sys.argv
    if '--rebuild-keys' in sys.argv:
        keys = build_keys()
        print('keys: %d files, %d runs' % (len(keys), sum(len(v) for v in keys.values())))
        return
    stats = {'same': 0, 'changed': []}
    hues, problems, dds_info = [], [], []
    keys = load_keys()
    for p in red_files('.part'):
        make_part(p, keys, stats, check, hues)
    for p in red_files('.xdb'):
        make_xdb(p, stats, check, hues, problems)
    for p in red_files('.dds'):
        make_dds(p, stats, check, dds_info)
    print('written %d files, %d unchanged%s' % (len(stats['changed']), stats['same'], ' (check only)' if check else ''))
    for r, mode in dds_info:
        print('  dds', r, mode)
    for r, old, new in hues:
        if len(old):
            o, n = old[0], new[0]
            print('  %-90s %5d colours, e.g. %s -> %s' % (r, len(old), hsv(o), hsv(n)))
    for pr in problems:
        print('PROBLEM', pr)


if __name__ == '__main__':
    main()
