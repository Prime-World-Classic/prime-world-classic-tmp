#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""frame_ab.py — сверка кадров двух контуров по метрикам, пригодным для
обзорной камеры (дырки в террейне, тон, туман) и для обычной.

Запуск только /usr/bin/python3 (PIL в /usr/lib/python3/dist-packages).

  frame_ab.py stats IMG [IMG...]            свод по кадру
  frame_ab.py diff  A B [--grid NXNY]       поканальная дельта + регионы
  frame_ab.py holes IMG [--thr 12] [--min 400]
        связные компоненты почти-чёрного: число, площадь, bbox, fill ratio.
        Для обзора уровня прямоугольные компоненты с fill>0.7 = дырки террейна.
  frame_ab.py fog IMG [--band Y0:Y1]        профиль верхней полосы: средняя
        светимость по строкам + доля пикселей, совпадающих с модальным цветом
        середины кадра (признак «туман лежит на вьюпорте, а не на террейне»)
  frame_ab.py crop  IMG X0 Y0 X1 Y1 OUT    вырез для просмотра
  frame_ab.py rows  IMG X0 Y0 X1 Y1         средняя светимость по строкам региона

Метрики: mean/p1/p50/p99 по светимости (0.299R+0.587G+0.114B), доля чёрного
(все каналы < --black, дефолт 12), доля белого (все > 245), уникальные цвета
(квант 4 бита на канал), средний B-R.
"""
import sys, os
import numpy as np
from PIL import Image

BLACK = 12


def load(p):
    return np.asarray(Image.open(p).convert('RGB'), dtype=np.uint8)


def lum(a):
    return 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]


def stats(p, black=BLACK):
    a = load(p); L = lum(a)
    q = (a >> 4).astype(np.uint16)
    key = (q[..., 0] << 8) | (q[..., 1] << 4) | q[..., 2]
    blk = (a < black).all(axis=2).mean() * 100
    wht = (a > 245).all(axis=2).mean() * 100
    br = float(a[..., 2].astype(np.int32).mean() - a[..., 0].astype(np.int32).mean())
    return dict(path=p, shape=a.shape, mean=float(L.mean()), p1=float(np.percentile(L, 1)),
                p50=float(np.percentile(L, 50)), p99=float(np.percentile(L, 99)),
                black=blk, white=wht, uniq=int(len(np.unique(key))), br=br)


def print_stats(paths, black=BLACK):
    hdr = ('file', 'shape', 'mean', 'p1', 'p50', 'p99', '%black', '%white', 'uniq', 'B-R')
    print('%-46s %-11s %6s %5s %5s %5s %7s %7s %6s %6s' % hdr)
    for p in paths:
        s = stats(p, black)
        print('%-46s %-11s %6.2f %5.1f %5.1f %5.1f %7.2f %7.2f %6d %6.1f' % (
            os.path.basename(p), '%dx%d' % (s['shape'][1], s['shape'][0]), s['mean'],
            s['p1'], s['p50'], s['p99'], s['black'], s['white'], s['uniq'], s['br']))


def diff(a_path, b_path, grid=None, tol=8, gross=64):
    A, B = load(a_path), load(b_path)
    if A.shape != B.shape:
        print('РАЗНЫЕ РАЗМЕРЫ %s %s' % (A.shape, B.shape)); return
    d = np.abs(A.astype(np.int16) - B.astype(np.int16)).max(axis=2)
    H, W = d.shape
    print('gross>64 = %.2f%%  diff>8 = %.2f%%  max|d| = %d  mean|d| = %.2f' % (
        (d > gross).mean() * 100, (d > tol).mean() * 100, d.max(), d.mean()))
    if grid:
        nx, ny = grid
        print('grid %dx%d, gross>64 %% (строки сверху вниз):' % (nx, ny))
        ys = np.linspace(0, H, ny + 1).astype(int); xs = np.linspace(0, W, nx + 1).astype(int)
        head = '      ' + ''.join('%7d' % xs[i] for i in range(nx))
        print(head)
        for j in range(ny):
            row = ''.join('%7.1f' % ((d[ys[j]:ys[j + 1], xs[i]:xs[i + 1]] > gross).mean() * 100)
                          for i in range(nx))
            print('%6d%s' % (ys[j], row))


def holes(p, thr=BLACK, minarea=400):
    from scipy import ndimage
    a = load(p)
    m = (a < thr).all(axis=2)
    lab, n = ndimage.label(m, structure=np.ones((3, 3)))
    out = []
    if n:
        sizes = ndimage.sum(m, lab, range(1, n + 1))
        objs = ndimage.find_objects(lab)
        for i, sl in enumerate(objs):
            s = int(sizes[i])
            if s < minarea:
                continue
            y0, y1 = sl[0].start, sl[0].stop; x0, x1 = sl[1].start, sl[1].stop
            fill = s / float((y1 - y0) * (x1 - x0))
            out.append((s, x0, y0, x1, y1, fill))
    out.sort(reverse=True)
    tot = sum(o[0] for o in out)
    print('%s: чёрных связных компонентов >=%d px: %d, суммарно %d px (%.2f%% кадра)'
          % (os.path.basename(p), minarea, len(out), tot, tot / float(m.size) * 100))
    for s, x0, y0, x1, y1, fill in out[:15]:
        print('   area=%7d bbox=(%4d,%4d)-(%4d,%4d) %4dx%-4d fill=%.2f'
              % (s, x0, y0, x1, y1, x1 - x0, y1 - y0, fill))
    return out


def rows(p, x0, y0, x1, y1):
    a = load(p); L = lum(a[y0:y1, x0:x1])
    print('%s: средняя светимость строк %d:%d в x %d:%d' % (os.path.basename(p), y0, y1, x0, x1))
    prof = L.mean(axis=1)
    for i in range(0, y1 - y0, 8):
        print('   y=%4d %6.1f %s' % (y0 + i, prof[i], '#' * int(prof[i] // 2)))


def fog(p, band=(0, 60), mid=(300, 420)):
    """доля пикселей верхней полосы, близких к модальному цвету середины кадра"""
    a = load(p).astype(np.int16)
    reg = a[mid[0]:mid[1], :]
    q = (reg >> 3).reshape(-1, 3)
    key = (q[:, 0] << 6) | (q[:, 1] << 3) | q[:, 2]
    vals, cnts = np.unique(key, return_counts=True)
    modal = vals[cnts.argmax()]
    mc = np.array([(modal >> 6) & 7, (modal >> 3) & 7, modal & 7]) * 8 + 4
    top = a[band[0]:band[1], :]
    dd = np.abs(top - mc).max(axis=2)
    print('%s: модальный цвет середины = %s; верхняя полоса %d:%d -> доля близких '
          '(<24) = %.1f%%, средняя светимость = %.1f'
          % (os.path.basename(p), tuple(int(v) for v in mc), band[0], band[1],
             (dd < 24).mean() * 100, lum(top).mean()))


def crop(p, x0, y0, x1, y1, out):
    Image.open(p).convert('RGB').crop((x0, y0, x1, y1)).save(out)
    print('crop %s -> %s (%dx%d)' % (os.path.basename(p), out, x1 - x0, y1 - y0))


def argv(flags, args, name, cast=str, default=None):
    if name in args:
        i = args.index(name)
        v = cast(args[i + 1]); args = args[:i] + args[i + 2:]
        return v, args
    return default, args


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    cmd, args = sys.argv[1], sys.argv[2:]
    thr, args = argv(args, args, '--thr', int, BLACK)
    mn, args = argv(args, args, '--min', int, 400)
    g, args = argv(args, args, '--grid', str)
    band, args = argv(args, args, '--band', str)
    if cmd == 'stats':
        print_stats(args, thr)
    elif cmd == 'diff':
        grid = tuple(int(v) for v in g.split('x')) if g else None
        diff(args[0], args[1], grid)
    elif cmd == 'holes':
        for p in args:
            holes(p, thr, mn)
    elif cmd == 'rows':
        rows(args[0], *[int(v) for v in args[1:5]])
    elif cmd == 'fog':
        b = tuple(int(v) for v in band.split(':')) if band else (0, 60)
        for p in args:
            fog(p, b)
    elif cmd == 'crop':
        crop(args[0], *[int(v) for v in args[1:5]], args[5])
    else:
        print(__doc__); sys.exit(1)
