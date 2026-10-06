#!/usr/bin/env python3
# A newc cpio of a tree, owners from a list ("path uid gid"; the rest root's), hard links kept.
import os, stat, sys

root, owners_file, out = sys.argv[1], sys.argv[2], sys.argv[3]
owners = {}
for line in open(owners_file):
    f = line.split('#')[0].split()
    if len(f) == 3:
        owners[f[0].lstrip('/')] = (int(f[1]), int(f[2]))

entries = []
for dirpath, dirnames, filenames in os.walk(root):
    dirnames.sort()
    for name in [''] + sorted(filenames + [d for d in dirnames if os.path.islink(os.path.join(dirpath, d))]):
        path = os.path.join(dirpath, name) if name else dirpath
        rel = os.path.relpath(path, root)
        entries.append('.' if rel == '.' else './' + rel)
entries = sorted(set(entries))

# Hard links: the same inode gets one number and its data only on the last name.
links, last = {}, {}
for e in entries:
    st = os.lstat(os.path.join(root, e))
    if not stat.S_ISDIR(st.st_mode) and st.st_nlink > 1:
        links.setdefault((st.st_dev, st.st_ino), []).append(e)
for names in links.values():
    last[names[-1]] = True

def header(ino, mode, uid, gid, nlink, mtime, size, name):
    fields = [ino, mode, uid, gid, nlink, mtime, size, 0, 0, 0, 0, len(name) + 1, 0]
    return b'070701' + b''.join(b'%08x' % v for v in fields)

def pad(n):
    return b'\0' * ((4 - n % 4) % 4)

with open(out, 'wb') as f:
    inos = {}
    for e in entries:
        p = os.path.join(root, e)
        st = os.lstat(p)
        key = (st.st_dev, st.st_ino)
        ino = inos.setdefault(key, len(inos) + 1)
        uid, gid = owners.get(e[2:] if e.startswith('./') else '', (0, 0))
        if stat.S_ISLNK(st.st_mode):
            data = os.readlink(p).encode()
        elif stat.S_ISREG(st.st_mode) and (st.st_nlink == 1 or e in last):
            data = open(p, 'rb').read()
        else:
            data = b''
        nlink = st.st_nlink if not stat.S_ISDIR(st.st_mode) else 2
        name = e.encode()
        h = header(ino, st.st_mode, uid, gid, nlink, int(st.st_mtime), len(data), name)
        f.write(h + name + b'\0' + pad(len(h) + len(name) + 1) + data + pad(len(data)))
    t = header(0, 0, 0, 0, 1, 0, 0, b'TRAILER!!!')
    f.write(t + b'TRAILER!!!\0' + pad(len(t) + 11))
