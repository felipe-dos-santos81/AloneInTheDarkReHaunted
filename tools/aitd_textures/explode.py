# SPDX-License-Identifier: GPL-2.0-only
# Ported unchanged from m-aitd (PyAitD/engine/data/explode.py), GPL-2.0-only.
"""PKWARE "explode" decompression (PAK compression flag 1).

Faithful translation of FITD FitdLib/unpack.cpp (GPLv2), itself based on
Mark Adler's 1992 unzip code.
"""

BMAX = 16
WSIZE = 0x8000
_MASKS = [(1 << i) - 1 for i in range(17)]
_CPLEN2 = list(range(2, 66))
_CPLEN3 = list(range(3, 67))
_EXTRA = [0] * 63 + [8]
_DIST4 = [1 + 64 * i for i in range(64)]
_DIST8 = [1 + 128 * i for i in range(64)]


class ExplodeError(ValueError):
    pass


def _get_tree(src, pos, n):
    lengths = [0] * n
    pairs = src[pos] + 1
    pos += 1
    k = 0
    while True:
        if pos >= len(src):
            raise ExplodeError("truncated tree")
        j = src[pos]
        pos += 1
        bits = (j & 0x0F) + 1
        count = ((j & 0xF0) >> 4) + 1
        if k + count > n:
            raise ExplodeError("tree overflow")
        for _ in range(count):
            lengths[k] = bits
            k += 1
        pairs -= 1
        if pairs == 0:
            break
    if k != n:
        raise ExplodeError("tree size mismatch")
    return pos, lengths


class _Entry:
    __slots__ = ("b", "e", "v")

    def __init__(self, b, e, v):
        self.b = b
        self.e = e
        self.v = v


def _huft_build(lengths, n, s, d, e, m):
    c = [0] * (BMAX + 1)
    for i in range(n):
        c[lengths[i]] += 1
    if c[0] == n:
        return None, 0
    el = lengths[256] if n > 256 else BMAX
    j = 1
    while c[j] == 0:
        j += 1
    k = j
    m = max(m, j)
    i = BMAX
    while c[i] == 0:
        i -= 1
    g = i
    m = min(m, i)
    y = 1 << j
    while j < i:
        y -= c[j]
        if y < 0:
            raise ExplodeError("more codes than bits")
        j += 1
        y <<= 1
    y -= c[i]
    if y < 0:
        raise ExplodeError("more codes than bits")
    c[i] += y

    x = [0] * (BMAX + 1)
    j = 0
    p = 1
    xp = 2
    i = g
    while i > 1:
        j += c[p]
        x[xp] = j
        p += 1
        xp += 1
        i -= 1

    v = [0] * 288
    i = 0
    while i < n:
        ln = lengths[i]
        if ln != 0:
            v[x[ln]] = i
            x[ln] += 1
        i += 1
    n = x[g]

    x[0] = 0
    i = 0
    h = -1
    lvl = [0] * (BMAX + 1)  # lvl[h+1] == C l[h]
    w = 0
    u = [None] * BMAX
    root = None
    pv = 0
    k2 = k
    while k2 <= g:
        a = c[k2]
        while a > 0:
            a -= 1
            while k2 > w + lvl[h + 1]:
                w += lvl[h + 1]
                h += 1
                z = g - w
                if z > m:
                    z = m
                j2 = k2 - w
                f = 1 << j2
                if f > a + 1:
                    f -= a + 1
                    xp = k2
                    while True:
                        j2 += 1
                        if j2 >= z:
                            break
                        f <<= 1
                        if f <= c[xp + 1]:
                            break
                        f -= c[xp + 1]
                        xp += 1
                if w + j2 > el and w < el:
                    j2 = el - w
                z = 1 << j2
                lvl[h + 1] = j2
                q = [None] * z
                u[h] = q
                if root is None:
                    root = q
                if h:
                    x[h] = i
                    jj = (i & ((1 << w) - 1)) >> (w - lvl[h])
                    u[h - 1][jj] = _Entry(lvl[h], 32 + j2, q)
            r = _Entry(k2 - w, 0, 0)
            if pv >= n:
                r.e = 99
            elif v[pv] < s:
                r.e = 32 if v[pv] < 256 else 31
                r.v = v[pv]
                pv += 1
            else:
                r.e = e[v[pv] - s]
                r.v = d[v[pv] - s]
                pv += 1
            q = u[h]
            f = 1 << (k2 - w)
            jj = i >> w
            while jj < len(q):
                q[jj] = r
                jj += f
            jj = 1 << (k2 - 1)
            while i & jj:
                i ^= jj
                jj >>= 1
            i ^= jj
            while (i & ((1 << w) - 1)) != x[h]:
                h -= 1
                w -= lvl[h + 1]
        k2 += 1
    return root, lvl[1]


def explode(src, uncompressed_size, flags):
    src = bytes(src)
    bl = 7
    bd = 8 if len(src) > 200000 else 7
    if flags & 4:
        pos, lengths = _get_tree(src, 0, 256)
        tb, _ = _huft_build(lengths, 256, 256, None, None, 9)
        pos, lengths = _get_tree(src, pos, 64)
        tl, bl = _huft_build(lengths, 64, 0, _CPLEN3, _EXTRA, bl)
    else:
        tb = None
        pos, lengths = _get_tree(src, 0, 64)
        tl, bl = _huft_build(lengths, 64, 0, _CPLEN2, _EXTRA, bl)
    if tl is None:
        raise ExplodeError("invalid literal/length tree")
    pos, lengths = _get_tree(src, pos, 64)
    if flags & 2:
        bdl = 7
        td, _ = _huft_build(lengths, 64, 0, _DIST8, _EXTRA, bd)
    else:
        bdl = 6
        td, _ = _huft_build(lengths, 64, 0, _DIST4, _EXTRA, bd)
    if td is None:
        raise ExplodeError("invalid distance tree")

    bitbuf = 0
    nbits = 0
    spos = pos

    def need(n):
        nonlocal bitbuf, nbits, spos
        while nbits < n:
            if spos < len(src):
                bitbuf |= src[spos] << nbits
                spos += 1
            nbits += 8

    def dump(n):
        nonlocal bitbuf, nbits
        bitbuf >>= n
        nbits -= n

    def decode_huft(table, bits, mask):
        need(bits)
        t = table[(~bitbuf) & mask]
        while True:
            if t is None:
                raise ExplodeError("invalid code")
            dump(t.b)
            e2 = t.e
            if e2 <= 32:
                return e2, t.v
            if e2 == 99:
                raise ExplodeError("invalid code")
            e2 &= 31
            need(e2)
            t = t.v[(~bitbuf) & _MASKS[e2]]

    slide = bytearray(WSIZE)
    out = bytearray()
    w = 0
    unflushed = 1
    remaining = uncompressed_size
    ml = _MASKS[bl]
    md = _MASKS[bd]
    mdl = _MASKS[bdl]
    while remaining > 0:
        need(1)
        if bitbuf & 1:
            dump(1)
            remaining -= 1
            if tb is not None:
                _, val = decode_huft(tb, 9, _MASKS[9])
            else:
                need(8)
                val = bitbuf & 0xFF
                dump(8)
            slide[w] = val
            w += 1
            if w == WSIZE:
                out += slide
                w = 0
                unflushed = 0
        else:
            dump(1)
            need(bdl)
            d = bitbuf & mdl
            dump(bdl)
            _, dhi = decode_huft(td, bd, md)
            d = w - d - dhi
            e2, ln = decode_huft(tl, bl, ml)
            if e2:
                need(8)
                ln += bitbuf & 0xFF
                dump(8)
            remaining = remaining - ln if remaining > ln else 0
            n = ln
            while True:
                d &= WSIZE - 1
                chunk = WSIZE - (d if d > w else w)
                if chunk > n:
                    chunk = n
                n -= chunk
                if unflushed and w <= d:
                    for _ in range(chunk):
                        slide[w] = 0
                        w += 1
                    d += chunk
                elif w - d >= chunk:
                    slide[w : w + chunk] = slide[d : d + chunk]
                    w += chunk
                    d += chunk
                else:
                    for _ in range(chunk):
                        slide[w] = slide[d]
                        w += 1
                        d += 1
                if w == WSIZE:
                    out += slide
                    w = 0
                    unflushed = 0
                if n == 0:
                    break
    out += slide[:w]
    if len(out) != uncompressed_size:
        raise ExplodeError(f"size mismatch: got {len(out)}, want {uncompressed_size}")
    return bytes(out)
