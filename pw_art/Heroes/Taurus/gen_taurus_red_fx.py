"""Taurus: reddish skill effects for the enemies (allies keep the yellow ones), like Laguna's _Red/_Team effects.

usage: py -3 gen_taurus_red_fx.py            (from anywhere; paths are taken from the repository)

For every effect of COLORED:
  Heroes/Taurus/Effects/<name>_Red.PRXY   copy of <name>.PRXY: new GUIDs, colours of the listed components turned red,
                                           their yellow textures replaced by the red ones (Heroes/Taurus/EffectsRed/Tex)
  Heroes/Taurus/Effects/<name>_Red.EFFT   copy of <name>.EFFT on the red PRXY
  Heroes/Taurus/Effects/<name>_Team.EFFT  EffectSwitcher: allies of Taurus see <name>, enemies - <name>_Red
The talents and _.HROB then refer to <name>_Team instead of <name>; BackLinks are written as the editor does.
Red textures: the DXT5 blocks are recoloured in place (only the end colours of each block), so the alpha, the
block indices and all mip levels stay exactly as they were. The particles (.part/.stat) are not touched: their
colour comes from the PRXY materials and textures.
Running it again rebuilds the same files (the GUIDs of the existing red PRXY are kept).
"""
import colorsys, os, re, struct, uuid

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
DATA = os.path.join(REPO, 'pw', 'branches', 'r1117', 'Data')
FX = '/Heroes/Taurus/Effects/'
TEX = '/Heroes/Taurus/Effects/Tex/'
RED_TEX = '/Heroes/Taurus/EffectsRed/Tex/'

# effect -> captions of the components to recolour (None = all of them); dust, rocks, craters and hoof prints stay
COLORED = {
    'ChargeRing': None,
    'Hit': None,
    'JumpMark': None,
    'JumpCharge': ['JumpCharge_Sparks'],
    'JumpLaunch': ['JumpLaunch_GroundFlash', 'JumpLaunch_Flash'],
    'JumpLand': ['JumpLand_ShockRing', 'JumpLand_GroundFlash', 'JumpLand_Flash', 'JumpLand_CracksFlash'],
    'RamImpact': ['RamImpact_Flash', 'RamImpact_Sparks'],
    'RamGlowL': None,
    'RamGlowR': None,
    'Stomp1': ['Stomp1_Flash'],
    'Stomp2': ['Stomp2_Flash'],
    'Stomp3': ['Stomp3_Flash'],
    'Stomp4': ['Stomp4_Flash'],
}
# applied on the target of the attack: the receiver is the enemy of Taurus, so "visible" (receiver of the player's
# faction) is the enemy's view. The other effects are on Taurus himself or on the ground (switched by the sender).
ON_VICTIM = {'Hit'}

# yellow / orange textures of the skills (the rest are white, grey or brown and get their colour from the materials)
COLORED_TEXTURES = ['ChargeRingLine', 'ChargeRingSoft', 'HitCenterGlow', 'HitRays', 'HoofGlowL', 'HoofGlowR',
                    'RamFlash', 'StompCracksFlash']

REFERRERS = ['Heroes/Taurus/_.HROB.xdb'] + ['Items/Talents/Class/Taurus/' + f for f in
             sorted(os.listdir(os.path.join(DATA, 'Items', 'Talents', 'Class', 'Taurus'))) if f.endswith('.xdb')]


def redden(r, g, b):
    """yellow / orange (hue up to 75 degrees) -> reddish (hue about 8, a warm red, not a pure one); the saturation
    and brightness stay, other colours and greys stay"""
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    deg = h * 360.0
    if deg > 180.0:
        deg -= 360.0
    if s < 0.08 or not (-15.0 <= deg <= 75.0):
        return r, g, b
    nh = (8.0 + (deg - 38.0) * 0.3) % 360.0
    return colorsys.hsv_to_rgb(nh / 360.0, s, v)


def path(dbid):
    return os.path.join(DATA, *dbid.strip('/').split('/'))


def read(p):
    with open(p, 'rb') as f:
        b = f.read()
    assert b.startswith(b'\xef\xbb\xbf'), p
    return b[3:].decode('utf-8')


def write(p, text):
    text = text.replace('\r\n', '\n').replace('\n', '\r\n')
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, 'wb') as f:
        f.write(b'\xef\xbb\xbf' + text.encode('utf-8'))


def fmt(x):
    s = ('%.4f' % x).rstrip('0').rstrip('.')
    return s if s not in ('-0', '') else '0'


# ---------------------------------------------------------------------------------------------------- textures

def rgb565(c):
    return ((c >> 11) & 31) / 31.0, ((c >> 5) & 63) / 63.0, (c & 31) / 31.0


def to565(r, g, b):
    q = lambda x, m: max(0, min(m, int(round(x * m))))
    return (q(r, 31) << 11) | (q(g, 63) << 5) | q(b, 31)


def recolor_dds(src, dst):
    b = bytearray(open(src, 'rb').read())
    assert b[:4] == b'DDS ' and b[84:88] == b'DXT5', src
    assert (len(b) - 128) % 16 == 0, src
    for off in range(128 + 8, len(b), 16):  # colour half of every BC3 block (always the 4-colour mode)
        c0, c1 = struct.unpack_from('<HH', b, off)
        struct.pack_into('<HH', b, off, to565(*redden(*rgb565(c0))), to565(*redden(*rgb565(c1))))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, 'wb').write(bytes(b))


# ---------------------------------------------------------------------------------------------------- backlinks

BACKLINK = re.compile(r'\t<BackLink DBID="([^"]*)" Count="(\d+)" />\r?\n')


def set_backlinks(text, links):
    """links: {dbid: count}; replaces all BackLinks of the root. The present ones keep their order (some lists are
    not sorted), a new one goes before the first present one that sorts after it, as the editor adds them"""
    order = [k for k in (m.group(1) for m in BACKLINK.finditer(text)) if k in links]
    for k in sorted(links):
        if k not in order:
            pos = next((i for i, o in enumerate(order) if o > k), len(order))
            order.insert(pos, k)
    text = BACKLINK.sub('', text)
    m = re.search(r'<\?xml[^>]*\?>\r?\n<\w+>\r?\n', text)
    lines = ''.join('\t<BackLink DBID="%s" Count="%d" />\r\n' % (k, links[k]) for k in order if links[k] > 0)
    return text[:m.end()] + lines + text[m.end():]


def get_backlinks(text):
    return {m.group(1): int(m.group(2)) for m in BACKLINK.finditer(text)}


# ---------------------------------------------------------------------------------------------------- effects

COLOR = re.compile(r'(<(\w+)>\s*<A>([^<]*)</A>\s*<B>)([^<]*)(</B>\s*<G>)([^<]*)(</G>\s*<R>)([^<]*)(</R>)')


def recolor_chunk(chunk):
    def sub(m):
        r, g, b = float(m.group(8)), float(m.group(6)), float(m.group(4))
        nr, ng, nb = redden(r, g, b)
        if (nr, ng, nb) == (r, g, b):
            return m.group(0)
        return m.group(1) + fmt(nb) + m.group(5) + fmt(ng) + m.group(7) + fmt(nr) + m.group(9)
    chunk = COLOR.sub(sub, chunk)
    for t in COLORED_TEXTURES:
        chunk = chunk.replace('href="%s%s.TXTR"' % (TEX, t), 'href="%s%s_Red.TXTR"' % (RED_TEX, t))
    return chunk


def make_red_prxy(name, captions):
    src = read(path(FX + name + '.PRXY.xdb'))
    dst_path = path(FX + name + '_Red.PRXY.xdb')
    old_ids = re.findall(r'id="([0-9A-F-]{36})"', read(dst_path)) if os.path.exists(dst_path) else []

    # top level components: items of <attached> at two tabs
    parts = re.split(r'(?m)(?=^\t\t<Item>\r?$)', src)
    used = set()
    for i in range(1, len(parts)):
        cap = re.search(r'<caption>([^<]*)</caption>', parts[i])
        cap = cap.group(1) if cap else ''
        if captions is None or cap in captions:
            parts[i] = recolor_chunk(parts[i])
            used.add(cap)
    if captions is not None:
        assert used == set(captions), (name, captions, used)
    text = ''.join(parts)

    text = text.replace('href="%s%s.PRXY"' % (FX, name), 'href="%s%s_Red.PRXY"' % (FX, name))
    ids = iter(old_ids)
    text = re.sub(r'id="[0-9A-F-]{36}"', lambda m: 'id="%s"' % (next(ids, None) or str(uuid.uuid4()).upper()), text)
    text = set_backlinks(text, {FX + name + '_Red.EFFT': 1})
    write(dst_path, text)
    return text


def make_effects(name):
    effT = read(path(FX + name + '.EFFT.xdb'))
    links = get_backlinks(effT)
    if FX + name + '_Team.EFFT' in links:  # already switched: the referrers are on the _Team file
        links = get_backlinks(read(path(FX + name + '_Team.EFFT.xdb')))
    assert links, name

    red = effT.replace('href="%s%s.PRXY"' % (FX, name), 'href="%s%s_Red.PRXY"' % (FX, name))
    write(path(FX + name + '_Red.EFFT.xdb'), set_backlinks(red, {FX + name + '_Team.EFFT': 1}))
    write(path(FX + name + '.EFFT.xdb'), set_backlinks(effT, {FX + name + '_Team.EFFT': 1}))

    normal, redfx = FX + name + '.EFFT', FX + name + '_Red.EFFT'
    if name in ON_VICTIM:
        logic, vis, inv = 'FactionVisibility', redfx, normal
    else:
        logic, vis, inv = 'SenderFaction', normal, redfx
    team = ('<?xml version="1.0" encoding="utf-8"?>\n<EffectSwitcher>\n'
            '\t<deathType>Manual</deathType>\n'
            '\t<effectOnInvisible href="%s" />\n'
            '\t<effectOnVisible href="%s" />\n'
            '\t<enableCondition href="" />\n'
            '\t<isVisibleUnderWarfog>false</isVisibleUnderWarfog>\n'
            '\t<lifeTime>0</lifeTime>\n'
            '\t<logic>%s</logic>\n'
            '</EffectSwitcher>' % (inv, vis, logic))
    write(path(FX + name + '_Team.EFFT.xdb'), set_backlinks(team, links))
    return links


def main():
    # red textures
    for t in COLORED_TEXTURES:
        recolor_dds(path(TEX + t + '.dds'), path(RED_TEX + t + '_Red.dds'))

    # red PRXY and the switchers
    tex_refs = {}  # texture dbid -> {prxy dbid: count}
    for name, captions in COLORED.items():
        text = make_red_prxy(name, captions)
        for m in re.finditer(r'<texture href="([^"]+)\.TXTR"', text):
            refs = tex_refs.setdefault(m.group(1), {})
            refs[FX + name + '_Red.PRXY'] = refs.get(FX + name + '_Red.PRXY', 0) + 1
        make_effects(name)

    # referrers: <name>.EFFT -> <name>_Team.EFFT
    for rel in REFERRERS:
        p = os.path.join(DATA, *rel.split('/'))
        text = read(p)
        new = text
        for name in COLORED:
            new = new.replace('href="%s%s.EFFT"' % (FX, name), 'href="%s%s_Team.EFFT"' % (FX, name))
        if new != text:
            write(p, new)
            print('refs', rel)

    # textures: red ones (new files), BackLinks of the red PRXY in every texture they use
    for t in COLORED_TEXTURES:
        src = read(path(TEX + t + '.TXTR.xdb'))
        src = src.replace('<textureFileName>%s%s.dds</textureFileName>' % (TEX, t),
                          '<textureFileName>%s%s_Red.dds</textureFileName>' % (RED_TEX, t))
        write(path(RED_TEX + t + '_Red.TXTR.xdb'), set_backlinks(src, {}))
    for tex, refs in sorted(tex_refs.items()):
        p = path(tex + '.TXTR.xdb')
        text = read(p)
        links = {k: v for k, v in get_backlinks(text).items() if not k.endswith('_Red.PRXY') or k not in refs}
        links.update(refs)
        write(p, set_backlinks(text, links))
    print('ok: %d effects, %d textures' % (len(COLORED), len(COLORED_TEXTURES)))


if __name__ == '__main__':
    main()
