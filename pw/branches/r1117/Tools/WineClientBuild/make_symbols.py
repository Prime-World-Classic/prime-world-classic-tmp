#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_symbols.py — компактная таблица имён функций PW_Game.exe для отчётов о крашах.

Из карты линкера (/MAP, 27 МБ текста) берутся только функции секции кода (0001),
имена раздекорированы (dbghelp UnDecorateSymbolName, «только имя»), и всё пишется
в PW_Game.sym.xz рядом с exe. Файл едет игрокам вместе с exe (game-pwc Bin/);
лаунчер при краше (content/modules/crashReport.js) переводит RVA кадров из
exception.log / fastfail.log в «Класс::Метод+0x1A» и шлёт это в отчёте.

Формат (UTF-8, после распаковки xz):
  # PW_Game.sym v1 timestamp=<TimeDateStamp exe, hex> size=<SizeOfImage, hex>
  <rva hex> <имя>          — по возрастанию rva
Лаунчер сверяет timestamp/size с заголовком установленного PW_Game.exe: таблица от
другой сборки не используется.

Использование:
  Tools/WineClientBuild/make_symbols.py <PW_Game.map> <PW_Game.exe> [-o PW_Game.sym.xz]
"""
import argparse
import ctypes
import lzma
import re
import struct
import sys

SYM_RE = re.compile(r'^ ([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9A-Fa-f]{8,16})\s')
BASE_RE = re.compile(r'Preferred load address is ([0-9A-Fa-f]+)')
STAMP_RE = re.compile(r'Timestamp is ([0-9A-Fa-f]+)')

UNDNAME_NAME_ONLY = 0x1000


def pe_header(exe):
    with open(exe, 'rb') as f:
        data = f.read(4096)
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise SystemExit(f'{exe}: не PE')
    stamp = struct.unpack_from('<I', data, pe + 8)[0]
    opt = pe + 24
    size = struct.unpack_from('<I', data, opt + 56)[0]
    return stamp, size


def undecorator():
    try:
        dbghelp = ctypes.windll.dbghelp
    except (AttributeError, OSError):
        return lambda name: name
    func = dbghelp.UnDecorateSymbolName
    func.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32]
    func.restype = ctypes.c_uint32
    buf = ctypes.create_string_buffer(2048)

    def undecorate(name):
        if not name.startswith('?'):
            return name
        n = func(name.encode('ascii', 'replace'), buf, len(buf), UNDNAME_NAME_ONLY)
        # « __ptr64» в шаблонных аргументах — шум для x64
        return buf.value[:n].decode('ascii', 'replace').replace(' __ptr64', '') if n else name

    return undecorate


def load_map(path):
    base = None
    stamp = None
    symbols = {}
    with open(path, 'r', encoding='latin-1') as f:
        for line in f:
            if base is None:
                m = BASE_RE.search(line)
                if m:
                    base = int(m.group(1), 16)
                    continue
            if stamp is None:
                m = STAMP_RE.search(line)
                if m:
                    stamp = int(m.group(1), 16)
                    continue
            m = SYM_RE.match(line)
            if not m or m.group(1) != '0001':
                continue
            name = m.group(3)
            # метки ($LN, $unwind...) и служебное — не функции
            if name.startswith('$') or name.startswith('__imp_'):
                continue
            addr = int(m.group(4), 16)
            if base is None or addr < base:
                continue
            symbols.setdefault(addr - base, name)
    if base is None:
        raise SystemExit(f'{path}: нет «Preferred load address»')
    return stamp, symbols


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('map')
    ap.add_argument('exe')
    ap.add_argument('-o', '--out', default=None)
    args = ap.parse_args()

    exe_stamp, exe_size = pe_header(args.exe)
    map_stamp, symbols = load_map(args.map)
    if map_stamp is not None and map_stamp != exe_stamp:
        raise SystemExit(f'карта от другой сборки: timestamp карты {map_stamp:08x}, exe {exe_stamp:08x}')

    undecorate = undecorator()
    lines = [f'# PW_Game.sym v1 timestamp={exe_stamp:08x} size={exe_size:x}']
    last = None
    for rva in sorted(symbols):
        name = undecorate(symbols[rva])
        if name == last:
            continue
        lines.append(f'{rva:x} {name}')
        last = name
    text = ('\n'.join(lines) + '\n').encode('utf-8')

    out = args.out or (args.exe[:-4] if args.exe.lower().endswith('.exe') else args.exe) + '.sym.xz'
    with open(out, 'wb') as f:
        f.write(lzma.compress(text, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, preset=9 | lzma.PRESET_EXTREME))
    print(f'{out}: {len(lines) - 1} функций, {len(text)} байт текста, timestamp={exe_stamp:08x} size={exe_size:x}')


if __name__ == '__main__':
    sys.exit(main())
