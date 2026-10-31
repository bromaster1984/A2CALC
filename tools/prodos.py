#!/usr/bin/env python3
"""Small strict ProDOS 140KiB image reader/writer; no third-party dependencies.

On-disk .po is block order. .dsk/.do is DOS 3.3 logical sector order, as used
by ADTPro. Physical nibble skew is intentionally left to the emulator/drive.
Only root-directory seedling and sapling files are written.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

BLOCK = 512
BLOCKS = 280
SIZE = BLOCK * BLOCKS
TEMPLATE_SHA256 = "398d333cb2ab92df9f8bb2cf64b946f2567116910eb8359cf4bdee5d4194f0fa"
# Each DOS logical sector's corresponding half-block in a ProDOS track.
DOS_TO_PO = (0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15)


def to_dos(po: bytes) -> bytes:
    if len(po) != SIZE:
        raise ValueError("a 35-track 140KiB image is required")
    out = bytearray(SIZE)
    for track in range(35):
        base = track * 4096
        for dos_sector, po_sector in enumerate(DOS_TO_PO):
            out[base + dos_sector * 256:base + (dos_sector + 1) * 256] = po[
                base + po_sector * 256:base + (po_sector + 1) * 256]
    return bytes(out)


def from_dos(dos: bytes) -> bytes:
    if len(dos) != SIZE:
        raise ValueError("a 35-track 140KiB image is required")
    out = bytearray(SIZE)
    for track in range(35):
        base = track * 4096
        for dos_sector, po_sector in enumerate(DOS_TO_PO):
            out[base + po_sector * 256:base + (po_sector + 1) * 256] = dos[
                base + dos_sector * 256:base + (dos_sector + 1) * 256]
    return bytes(out)


def u16(data: bytes, off: int) -> int:
    return int.from_bytes(data[off:off + 2], "little")


def u24(data: bytes, off: int) -> int:
    return int.from_bytes(data[off:off + 3], "little")


def applesingle_data(data: bytes) -> bytes:
    """Extract cc65's data fork, checking SYS type and $2000 load address."""
    if data[:8] != bytes.fromhex("0005160000020000"):
        raise ValueError("expected cc65 AppleSingle v2 executable")
    forks = {}
    for index in range(int.from_bytes(data[24:26], "big")):
        kind, offset, length = struct.unpack_from(">III", data, 26 + index * 12)
        if offset + length > len(data):
            raise ValueError("AppleSingle fork exceeds file")
        forks[kind] = data[offset:offset + length]
    if 1 not in forks or forks.get(11) != bytes.fromhex("00c300ff00002000"):
        raise ValueError("expected ProDOS SYS file at $2000")
    return forks[1]


class Volume:
    def __init__(self, image: bytes):
        if len(image) != SIZE:
            raise ValueError("a 35-track 140KiB image is required")
        self.image = bytearray(image)

    def directory_slots(self):
        """Yield real file slots, following the chain and excluding its header."""
        block = 2
        seen = set()
        while block:
            if block in seen or not 2 <= block < BLOCKS:
                raise ValueError("bad directory chain")
            seen.add(block)
            for entry in range(13):
                offset = block * BLOCK + 4 + entry * 39
                if block == 2 and entry == 0:
                    continue
                yield offset
            block = u16(self.image, block * BLOCK + 2)

    def entries(self):
        for offset in self.directory_slots():
            storage = self.image[offset] >> 4
            if storage == 0:
                continue
            length = self.image[offset] & 15
            name = self.image[offset + 1:offset + 1 + length].decode("ascii")
            yield name, storage, offset

    def read(self, name: str) -> bytes:
        for found, storage, offset in self.entries():
            if found != name:
                continue
            key = u16(self.image, offset + 17)
            size = u24(self.image, offset + 21)
            if storage == 1:
                return bytes(self.image[key * BLOCK:key * BLOCK + size])
            if storage != 2:
                raise ValueError("only seedling/sapling files are supported")
            result = bytearray()
            for index in range((size + 511) // 512):
                block = self.image[key * BLOCK + index] | (
                    self.image[key * BLOCK + 256 + index] << 8)
                if block:
                    result.extend(self.image[block * BLOCK:(block + 1) * BLOCK])
                else:
                    result.extend(bytes(BLOCK))
            return bytes(result[:size])
        raise KeyError(name)

    @classmethod
    def format(cls, name: str, boot: bytes = bytes(1024)) -> "Volume":
        validate_name(name)
        v = cls(bytes(SIZE))
        v.image[:1024] = boot[:1024]
        for block in range(2, 6):
            struct.pack_into("<HH", v.image, block * BLOCK,
                             block - 1 if block > 2 else 0,
                             block + 1 if block < 5 else 0)
        off = 2 * BLOCK + 4
        v.image[off] = 0xF0 | len(name)
        v.image[off + 1:off + 1 + len(name)] = name.encode("ascii")
        # Deterministic valid ProDOS date: 31 October 2026, 00:00.
        struct.pack_into("<H", v.image, off + 24, (26 << 9) | (10 << 5) | 31)
        v.image[off + 28:off + 33] = bytes((5, 0, 0xC3, 39, 13))
        struct.pack_into("<HHH", v.image, off + 33, 0, 6, BLOCKS)
        for block in range(7, BLOCKS):
            v.image[6 * BLOCK + block // 8] |= 0x80 >> (block % 8)
        return v

    def free(self, block: int) -> bool:
        return bool(self.image[6 * BLOCK + block // 8] & (0x80 >> (block % 8)))

    def allocate(self) -> int:
        for block in range(7, BLOCKS):
            if self.free(block):
                self.image[6 * BLOCK + block // 8] &= ~(0x80 >> (block % 8))
                self.image[block * BLOCK:(block + 1) * BLOCK] = bytes(BLOCK)
                return block
        raise ValueError("disk full")

    def add(self, name: str, data: bytes, filetype: int, aux: int = 0):
        validate_name(name)
        if any(found == name for found, _, _ in self.entries()):
            raise ValueError("duplicate filename: " + name)
        if len(data) > 128 * 1024:
            raise ValueError("file requires tree storage")
        count = u16(self.image, 2 * BLOCK + 4 + 33)
        # ProDOS safe-save operations can leave deleted slots before live files.
        # The file count is not the index of the final occupied directory slot.
        off = next((slot for slot in self.directory_slots()
                    if self.image[slot] >> 4 == 0), None)
        if off is None:
            raise ValueError("root directory full")
        chunks = [data[i:i + BLOCK] for i in range(0, len(data), BLOCK)]
        needed = len(chunks) + (len(chunks) > 1)
        if sum(self.free(block) for block in range(7, BLOCKS)) < needed:
            raise ValueError("disk full")
        blocks = [self.allocate() for _ in chunks]
        for block, chunk in zip(blocks, chunks):
            self.image[block * BLOCK:block * BLOCK + len(chunk)] = chunk
        storage = 1
        key = blocks[0] if blocks else 0
        used = len(blocks)
        if len(blocks) > 1:
            storage = 2
            key = self.allocate()
            used += 1
            for index, block in enumerate(blocks):
                self.image[key * BLOCK + index] = block & 255
                self.image[key * BLOCK + 256 + index] = block >> 8
        self.image[off:off + 39] = bytes(39)
        self.image[off] = storage << 4 | len(name)
        self.image[off + 1:off + 1 + len(name)] = name.encode("ascii")
        self.image[off + 16] = filetype
        struct.pack_into("<HH", self.image, off + 17, key, used)
        self.image[off + 21:off + 24] = len(data).to_bytes(3, "little")
        struct.pack_into("<H", self.image, off + 24, (26 << 9) | (10 << 5) | 31)
        self.image[off + 30] = 0xC3
        struct.pack_into("<H", self.image, off + 31, aux)
        struct.pack_into("<H", self.image, off + 33, (26 << 9) | (10 << 5) | 31)
        struct.pack_into("<H", self.image, off + 37, 2)
        struct.pack_into("<H", self.image, 2 * BLOCK + 4 + 33, count + 1)

    def verify(self):
        used = set(range(7))
        entries = list(self.entries())
        if len(entries) != u16(self.image, 2 * BLOCK + 4 + 33):
            raise ValueError("directory file count mismatch")
        for name, storage, off in entries:
            size = u24(self.image, off + 21)
            key = u16(self.image, off + 17)
            n = (size + BLOCK - 1) // BLOCK
            if storage == 1:
                blocks = [key] if n else []
                if n > 1:
                    raise ValueError("oversized seedling")
            elif storage == 2:
                blocks = [key] + [self.image[key * BLOCK + i] |
                                  self.image[key * BLOCK + 256 + i] << 8
                                  for i in range(n)]
            else:
                raise ValueError("unsupported storage")
            if len(blocks) != u16(self.image, off + 19):
                raise ValueError(name + " block count mismatch")
            for block in blocks:
                if not 7 <= block < BLOCKS or block in used:
                    raise ValueError(name + " invalid/shared allocation")
                used.add(block)
            if len(self.read(name)) != size:
                raise ValueError("readback length mismatch")
        for block in range(BLOCKS):
            if self.free(block) == (block in used):
                raise ValueError("allocation bitmap mismatch")
        return BLOCKS - len(used)


def validate_name(name):
    if not 1 <= len(name) <= 15 or not "A" <= name[0] <= "Z" or any(
            char not in "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789." for char in name):
        raise ValueError("invalid ProDOS name: " + name)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("image", type=Path)
    action = p.add_mutually_exclusive_group()
    action.add_argument("--extract", metavar="NAME", help="extract an existing file")
    action.add_argument("--add", type=Path, metavar="FILE", help="add a local text file to a new image")
    p.add_argument("--name", help="new ProDOS filename, e.g. IMPORT.A2C")
    p.add_argument("--output", type=Path, help="new output file; existing paths are never overwritten")
    args = p.parse_args()
    if args.name and args.add is None:
        p.error("--name requires --add")
    if args.add is not None and (not args.name or args.output is None):
        p.error("--add requires --name and --output")
    if args.extract and args.output is None:
        p.error("--extract requires --output")
    if args.output and not (args.extract or args.add is not None):
        p.error("--output requires --add or --extract")
    try:
        if args.output:
            if args.output.resolve() == args.image.resolve():
                raise ValueError("output must differ from the source image")
            if args.output.exists() or args.output.is_symlink():
                raise ValueError("output already exists; choose a new path")
        if args.image.suffix.lower() not in (".po", ".dsk", ".do"):
            raise ValueError("input image must use .dsk/.do (DOS order) or .po (block order)")
        raw = args.image.read_bytes()
        v = Volume(raw if args.image.suffix.lower() == ".po" else from_dos(raw))
        if args.extract:
            data = v.read(args.extract)
        elif args.add is not None:
            if args.output.suffix.lower() not in (".po", ".dsk", ".do"):
                raise ValueError("output image must use .dsk/.do or .po")
            # Check the original before touching its in-memory copy. Unsupported
            # layouts are refused rather than rewritten with guessed metadata.
            v.verify()
            added = args.add.read_bytes()
            originals = {name: v.read(name) for name, _, _ in v.entries()}
            v.add(args.name, added, 0x04)
            v.verify()
            if v.read(args.name) != added or any(v.read(name) != contents
                    for name, contents in originals.items()):
                raise ValueError("file readback/preservation check failed")
            data = bytes(v.image)
            if args.output.suffix.lower() != ".po":
                data = to_dos(data)
        else:
            for name, kind, off in v.entries():
                print(f"{name:15} ${v.image[off + 16]:02X} {u24(v.image, off + 21):6} bytes")
            return
        # Exclusive creation closes the exists/open race and never truncates a
        # pre-existing destination, even if another process creates it meanwhile.
        with args.output.open("xb") as output:
            output.write(data)
        print(f"Wrote {args.output} ({len(data)} bytes); source image preserved")
    except (OSError, ValueError, KeyError, UnicodeError) as error:
        p.exit(2, f"error: {error}\n")


if __name__ == "__main__":
    main()
