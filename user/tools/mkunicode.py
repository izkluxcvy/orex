#!/usr/bin/env python3
import sys
import unicodedata

ALPHA, UPPER, LOWER, SPACE, BLANK, CNTRL, PRINT, PUNCT = 1, 2, 4, 8, 16, 32, 64, 128
NOBREAK = (0xa0, 0x2007, 0x202f)  # spaces that must not split a line


def classes(c):
    ch = chr(c)
    cat = unicodedata.category(ch)
    b = 0
    if cat[0] == 'L' or cat == 'Nl':
        b |= ALPHA
    if cat in ('Lu', 'Lt'):
        b |= UPPER
    if cat == 'Ll':
        b |= LOWER
    if c in (9, 10, 11, 12, 13, 0x20, 0x85) or (cat in ('Zs', 'Zl', 'Zp') and c not in NOBREAK):
        b |= SPACE
    if c == 9 or (cat == 'Zs' and c not in NOBREAK):
        b |= BLANK
    if cat in ('Cc', 'Zl', 'Zp'):
        b |= CNTRL
    if cat not in ('Cc', 'Cs', 'Cn', 'Zl', 'Zp'):
        b |= PRINT
    if b & PRINT and not b & (ALPHA | SPACE) and not 0x30 <= c <= 0x39:
        b |= PUNCT
    return b


def width(c, b):
    cat = unicodedata.category(chr(c))
    if c == 0:
        return 0
    if not b & PRINT:
        return 3
    if cat in ('Mn', 'Me') or (cat == 'Cf' and c != 0xad) or 0x1160 <= c <= 0x11ff:
        return 0
    if unicodedata.east_asian_width(chr(c)) in ('W', 'F'):
        return 2
    return 1

def cases(f):
    delta = {}
    for c in range(0x110000):
        m = f(chr(c))
        if len(m) == 1 and ord(m) != c:
            delta[c] = ord(m) - c
    out, done = [], set()
    for c in sorted(delta):
        if c in done:
            continue
        best = (1, 0)
        for step in (1, 2):
            n = 0
            while delta.get(c + n * step) == delta[c]:
                n += 1
            if n > best[1]:
                best = (step, n)
        step, n = best
        out.append((c, n, delta[c], step))
        done.update(c + i * step for i in range(n))
    return out


def main():
    w = sys.stdout.write
    w("// Generated from Unicode %s (Python's unicodedata); do not edit.\n\n" % unicodedata.unidata_version)
    w('#include <stdint.h>\n\n')
    w('// From each start to the next: the classes in bits 21-28, the width in 29-30 (3: none).\n')
    w('const uint32_t __wc_class[] = {\n')
    last = None
    for c in range(0x110000):
        b = classes(c)
        v = b << 21 | width(c, b) << 29
        if v != last:
            w('    0x%08x,\n' % (v | c))
            last = v
    w('};\n')
    w('const unsigned __wc_class_n = sizeof __wc_class / sizeof __wc_class[0];\n\n')
    w('// Case: from start, count characters step apart map to c + delta.\n')
    for name, f in (('toupper', str.upper), ('tolower', str.lower)):
        w('const struct __wc_case __wc_%s[] = {\n' % name)
        for c, n, d, s in cases(f):
            w('    {0x%x, %d, %d, %d},\n' % (c, n, d, s))
        w('};\n')
        w('const unsigned __wc_%s_n = sizeof __wc_%s / sizeof __wc_%s[0];\n' % (name, name, name))


main()
