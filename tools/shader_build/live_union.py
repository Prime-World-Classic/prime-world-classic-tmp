#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""live_union.py — объединяет ЖИВые индексы пинов из трёх источников в один
снимок для `mkshd2.py --live`.

Зачем. `mkshd2.py` пишет секцию цели с нуля (`strip_trailer`): список живых
индексов обязан быть НАДМНОЖЕСТВОМ того, что уже лежит в контейнере, иначе
поставка потеряет варианты. Надмножество известно только из трёх мест:

  1. прежний снимок `live_indices-*.json` (сбор `collect_live_indices.py`);
  2. сами контейнеры `.shd` — секция цели tag=2 (SPIR-V) уже содержит индексы,
     которые когда-то попали в поставку (их никто, кроме файла, не помнит);
  3. свежие логи клиента с `setvar dump_shader_index 1`
     (`ShaderIndex: Shaders/UberShaders/<материал> #<индекс> (of <total>)`).

Использование:
  tools/shader_build/live_union.py --out /tmp/live_all.json \
      --json tools/shader_build/live_indices-2026-10-15-union.json \
      --containers pw/branches/r1117/Data/Shaders/UberShaders \
      --log <файл лога или каталог логов>...
"""
import argparse, json, os, re, struct, sys

LINE = re.compile(r"ShaderIndex:\s*Shaders/UberShaders/(\w+)\s+#(\d+)\s*(?:\(of\s+(\d+)\))?")
MISS = re.compile(r"ShaderTarget:\s*Shaders/UberShaders/(\w+)\s+#(\d+).*\u043a\u043e\u043c\u0431\u0438\u043d\u0430\u0446\u0438\u0438 \u043d\u0435\u0442 \u0432 \u0446\u0435\u043b\u0438")


def v2_indices(path):
    """Индексы секции tag=2 (SPIR-V) контейнера .shd; [] если v2 нет."""
    with open(path, 'rb') as f:
        b = f.read()
    if len(b) < 16 or b[-16:-12] != b'SHD2' or b[-4:] != b'SHD2':
        return []
    hdr_off = struct.unpack_from('<I', b, len(b) - 12)[0]
    if hdr_off + 16 > len(b) or b[hdr_off:hdr_off + 4] != b'V2H\0':
        return []
    _ver, _v1size, tcount = struct.unpack_from('<III', b, hdr_off + 4)
    out, p = [], hdr_off + 16
    for _ in range(tcount):
        tag, so, cnt = struct.unpack_from('<III', b, p); p += 12
        if tag != 2:
            continue
        q = so + 8
        for _i in range(cnt):
            idx = struct.unpack_from('<I', b, q)[0]; q += 20
            out.append(idx)
    return out


def add(mat, idx, total, store):
    e = store.setdefault(mat, {"indices": set(), "total": 0})
    e["indices"].add(int(idx))
    if total:
        e["total"] = max(e["total"], int(total))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--json", nargs="*", default=[])
    ap.add_argument("--containers", nargs="*", default=[])
    ap.add_argument("--log", nargs="*", default=[], help="файл лога или каталог")
    ap.add_argument("--missing", help="куда положить список отсутствующих индексов")
    a = ap.parse_args()

    store, sources = {}, []
    for j in a.json:
        d = json.load(open(j))
        m = d.get("materials", d)
        for name, e in m.items():
            for i in e.get("indices", []):
                add(name, i, e.get("total", 0), store)
        sources.append("json:" + os.path.basename(j))

    for d in a.containers:
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".shd"):
                continue
            name = fn[:-4]
            for i in v2_indices(os.path.join(d, fn)):
                add(name, i, 0, store)
        sources.append("containers:" + d)

    missing = {}
    logs = []
    for p in a.log:
        if os.path.isdir(p):
            logs += [os.path.join(p, f) for f in sorted(os.listdir(p))]
        else:
            logs.append(p)
    for lg in logs:
        try:
            with open(lg, 'rb') as f:
                for raw in f:
                    s = raw.decode('utf-8', 'replace')
                    if '\ufffd' in s:
                        # логи смешанной кодировки (cp1251/UTF-8) — кириллический
                        # маркер иначе не сматчится
                        s = raw.decode('cp1251', 'replace')
                    m = LINE.search(s)
                    if m:
                        add(m.group(1), m.group(2), m.group(3) or 0, store)
                        continue
                    m = MISS.search(s)
                    if m:
                        add(m.group(1), m.group(2), 0, store)
                        missing.setdefault(m.group(1), set()).add(int(m.group(2)))
        except OSError as e:
            print("пропущен лог %s: %s" % (lg, e), file=sys.stderr)
        sources.append("log:" + lg)

    out = {"sources": sources,
           "materials": {n: {"total": e["total"], "indices": sorted(e["indices"])}
                         for n, e in sorted(store.items())}}
    with open(a.out, "w") as f:
        json.dump(out, f, indent=1)
    tot = sum(len(e["indices"]) for e in out["materials"].values())
    print("материалов %d, индексов %d -> %s" % (len(out["materials"]), tot, a.out))
    if a.missing:
        with open(a.missing, "w") as f:
            json.dump({n: sorted(v) for n, v in sorted(missing.items())}, f, indent=1)
        print("отсутствующих (просили, но в цели их не было): %d материалов, %d индексов -> %s"
              % (len(missing), sum(len(v) for v in missing.values()), a.missing))


if __name__ == "__main__":
    main()
