#!/usr/bin/env python3
"""A disk a BIOS can start orex from, made on the host: a GUID partition table, the MBR's
code in sector 0, stage2 in the BIOS boot partition, and a FAT32 EFI system partition with
the files named. What orex's own gpt, fat and installboot do on a real disk.

  mkdisk.py OUT --mbr bootmbr.bin --stage2 bootbios.bin [--size MiB] [--esp MiB]
            [--root IMAGE] [NAME=file ...]          e.g. KERNEL.ELF=bin/kernel.elf
"""
import argparse
import struct
import sys
import uuid
import zlib

SECTOR = 512
MIB = 1024 * 1024 // SECTOR  # sectors in a MiB
STAGE2_LBA = 2048            # 1 MiB in: where partitions begin, by custom


def guid(s):
    return uuid.UUID(s).bytes_le


BIOS_BOOT = guid("21686148-6449-6e6f-744e-656564454649")
ESP = guid("c12a7328-f81f-11d2-ba4b-00a0c93ec93b")
LINUX = guid("0fc63daf-8483-4772-8e79-3d69d8477de4")


def name83(name):
    """KERNEL.ELF as the 11 bytes a directory entry holds: 'KERNEL  ELF'."""
    base, _, ext = name.upper().partition(".")
    if not 1 <= len(base) <= 8 or len(ext) > 3:
        sys.exit(f"mkdisk: {name}: not an 8.3 name")
    return base.ljust(8).encode() + ext.ljust(3).encode()

class Fat32:
    """A FAT32 file system built in memory: files and directories, 8.3 names alone."""

    def __init__(self, sectors):
        self.sectors = sectors
        mib = sectors // MIB
        self.spc = 1 if mib <= 260 else 8  # sectors per cluster
        self.rsvd, self.nfats = 32, 2
        t2 = (256 * self.spc + self.nfats) // 2
        self.fatsz = (sectors - self.rsvd + t2 - 1) // t2
        self.nclusters = (sectors - self.rsvd - self.nfats * self.fatsz) // self.spc
        if self.nclusters < 65525:
            sys.exit("mkdisk: the EFI system partition is too small for FAT32 (33 MiB at least)")
        self.data0 = self.rsvd + self.nfats * self.fatsz  # cluster 2's sector
        self.img = bytearray(sectors * SECTOR)
        self.fat = [0x0FFFFFF8, 0x0FFFFFFF]  # entries 0 and 1 are not clusters
        self.tree = {}                       # the root: name -> bytes, or a dict for a directory

    def add(self, path, data):
        d = self.tree
        *dirs, name = path.strip("/").split("/")
        for part in dirs:
            d = d.setdefault(part, {})
        d[name] = data

    def alloc(self, data):
        """Clusters for data, chained one after another; the first, or 0 for nothing."""
        size = self.spc * SECTOR
        n = (len(data) + size - 1) // size
        if n == 0:
            return 0
        first = len(self.fat)
        if first + n > self.nclusters + 2:
            sys.exit("mkdisk: the EFI system partition is full")
        self.fat += [first + i + 1 for i in range(n - 1)] + [0x0FFFFFFF]
        off = (self.data0 + (first - 2) * self.spc) * SECTOR
        self.img[off:off + len(data)] = data
        return first

    @staticmethod
    def entry(name11, attr, cluster, size):
        return struct.pack("<11sB8xHHHHI", name11, attr, cluster >> 16, 0, 0x21, cluster & 0xFFFF, size)

    def build_dir(self, tree, me, parent):
        """The directory's entries written into its cluster, its files and directories placed."""
        ents = b""
        if me != 2:  # all but the root begin with themselves and their parent
            ents += self.entry(b".          ", 0x10, me, 0)
            ents += self.entry(b"..         ", 0x10, 0 if parent == 2 else parent, 0)
        for name, item in tree.items():
            if isinstance(item, dict):
                c = self.alloc(bytes(self.spc * SECTOR))
                self.build_dir(item, c, me)
                ents += self.entry(name83(name), 0x10, c, 0)
            else:
                ents += self.entry(name83(name), 0x20, self.alloc(item), len(item))
        if len(ents) > self.spc * SECTOR:
            sys.exit("mkdisk: too many entries in one directory")
        off = (self.data0 + (me - 2) * self.spc) * SECTOR
        self.img[off:off + len(ents)] = ents

    def image(self):
        root = self.alloc(bytes(self.spc * SECTOR))  # cluster 2
        self.build_dir(self.tree, root, root)

        bpb = bytearray(SECTOR)
        bpb[0:3] = b"\xeb\x58\x90"
        bpb[3:11] = b"OREX    "
        struct.pack_into("<HBHB", bpb, 11, SECTOR, self.spc, self.rsvd, self.nfats)
        bpb[21] = 0xF8
        struct.pack_into("<HH", bpb, 24, 32, 64)
        struct.pack_into("<II", bpb, 32, self.sectors, self.fatsz)
        struct.pack_into("<I", bpb, 44, root)
        struct.pack_into("<HH", bpb, 48, 1, 6)  # FSInfo's sector, and the boot sector's copy
        bpb[64], bpb[66] = 0x80, 0x29
        struct.pack_into("<I", bpb, 67, 0x0E3E0001)
        bpb[71:82] = b"NO NAME    "
        bpb[82:90] = b"FAT32   "
        bpb[510:512] = b"\x55\xaa"

        info = bytearray(SECTOR)
        struct.pack_into("<I", info, 0, 0x41615252)
        struct.pack_into("<III", info, 484, 0x61417272, self.nclusters + 2 - len(self.fat), len(self.fat))
        struct.pack_into("<I", info, 508, 0xAA550000)

        for at in (0, 6):
            self.img[at * SECTOR:(at + 1) * SECTOR] = bpb
            self.img[(at + 1) * SECTOR:(at + 2) * SECTOR] = info
        table = struct.pack(f"<{len(self.fat)}I", *self.fat)
        for f in range(self.nfats):
            off = (self.rsvd + f * self.fatsz) * SECTOR
            self.img[off:off + len(table)] = table
        return self.img

def gpt(total, parts):
    """The protective MBR's entry, and both copies of the table: what goes where on the disk."""
    entries = bytearray(128 * 128)
    for i, (kind, first, last, name) in enumerate(parts):
        unique = uuid.uuid5(uuid.NAMESPACE_DNS, name + ".orex").bytes_le
        struct.pack_into("<16s16sQQQ72s", entries, i * 128, kind, unique, first, last, 0,
                         name.encode("utf-16-le"))
    disk = uuid.uuid5(uuid.NAMESPACE_DNS, "disk.orex").bytes_le

    def header(me, other, table):
        def pack(crc):
            return struct.pack("<8sIIIIQQQQ16sQIII", b"EFI PART", 0x10000, 92, crc, 0, me, other,
                               34, total - 34, disk, table, 128, 128, zlib.crc32(entries))
        return pack(zlib.crc32(pack(0))).ljust(SECTOR, b"\0")

    protective = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xEE, b"\xff\xff\xff", 1,
                             min(total - 1, 0xFFFFFFFF))
    return protective, {
        1: header(1, total - 1, 2),
        2: bytes(entries),
        total - 33: bytes(entries),
        total - 1: header(total - 1, 1, total - 33),
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--mbr", required=True)
    ap.add_argument("--stage2", required=True)
    ap.add_argument("--size", type=int, default=128, help="the disk's, in MiB")
    ap.add_argument("--esp", type=int, default=64, help="the EFI system partition's, in MiB")
    ap.add_argument("--root", help="a file system image, to be the third partition")
    ap.add_argument("files", nargs="*", help="NAME=file, into the EFI system partition")
    args = ap.parse_args()

    total = args.size * MIB
    with open(args.mbr, "rb") as f:
        mbr = f.read()
    with open(args.stage2, "rb") as f:
        stage2 = f.read()
    if len(mbr) > 440:
        sys.exit(f"mkdisk: {args.mbr} is more than 440 bytes")
    count = (len(stage2) + SECTOR - 1) // SECTOR
    if count > MIB:
        sys.exit(f"mkdisk: {args.stage2} is more than a MiB")

    # Sector 0: the code, told where stage2 is and how long, then the partition table's place.
    sector0 = bytearray(SECTOR)
    sector0[:len(mbr)] = mbr
    struct.pack_into("<QH", sector0, 0x1A8, STAGE2_LBA, count)
    sector0[510:512] = b"\x55\xaa"

    esp_first = STAGE2_LBA + MIB
    esp_last = esp_first + args.esp * MIB - 1
    parts = [(BIOS_BOOT, STAGE2_LBA, esp_first - 1, "boot"), (ESP, esp_first, esp_last, "esp")]
    root = b""
    if args.root:
        with open(args.root, "rb") as f:
            root = f.read()
        root_sectors = -(-len(root) // (MIB * SECTOR)) * MIB  # a whole number of MiB
        parts.append((LINUX, esp_last + 1, esp_last + root_sectors, "root"))
    if parts[-1][2] > total - 34:
        sys.exit("mkdisk: the partitions do not fit: a larger --size")
    protective, table = gpt(total, parts)
    sector0[446:462] = protective

    fat = Fat32(args.esp * MIB)
    for spec in args.files:
        name, _, path = spec.partition("=")
        with open(path, "rb") as f:
            fat.add(name, f.read())

    with open(args.out, "wb") as f:
        f.truncate(total * SECTOR)
        f.write(sector0)
        f.seek(STAGE2_LBA * SECTOR)
        f.write(stage2)
        for lba, data in table.items():
            f.seek(lba * SECTOR)
            f.write(data)
        f.seek(esp_first * SECTOR)
        f.write(fat.image())
        if root:
            f.seek((esp_last + 1) * SECTOR)
            f.write(root)


if __name__ == "__main__":
    main()
