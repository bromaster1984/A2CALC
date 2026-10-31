import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from prodos import Volume, to_dos, from_dos, SIZE


class ProdosTest(unittest.TestCase):
    def test_seedling_sapling_empty_and_bitmap(self):
        v = Volume.format("TEST")
        v.add("EMPTY", b"", 4)
        v.add("SMALL", b"abc", 4)
        large = bytes(range(256)) * 70
        v.add("LARGE", large, 0xFF, 0x2000)
        self.assertEqual(v.read("EMPTY"), b"")
        self.assertEqual(v.read("SMALL"), b"abc")
        self.assertEqual(v.read("LARGE"), large)
        self.assertEqual(v.verify(), 280 - 7 - 1 - 35 - 1)

    def test_sector_roundtrip(self):
        po = b"".join(bytes((i & 255,)) * 256 for i in range(SIZE // 256))
        dos = to_dos(po)
        self.assertEqual(from_dos(dos), po)
        self.assertEqual(dos[:256], po[:256])
        self.assertEqual(dos[256:512], po[14 * 256:15 * 256])

    def test_directory_continuation(self):
        v = Volume.format("TEST")
        for i in range(40):
            v.add("FILE" + str(i), str(i).encode(), 4)
        self.assertEqual(len(list(v.entries())), 40)
        self.assertEqual(v.read("FILE39"), b"39")
        v.verify()

    def test_detect_bad_bitmap(self):
        v = Volume.format("TEST")
        v.add("ABC", b"abc", 4)
        v.image[6 * 512] ^= 1
        with self.assertRaises(ValueError):
            v.verify()

    def test_reuse_deleted_slot_preserves_later_file(self):
        v = Volume.format("DATA")
        v.add("OLD", b"obsolete", 4)
        v.add("KEEP", b"important user data", 4)
        entries = list(v.entries())
        _, _, deleted = entries[0]
        _, _, keep = entries[1]
        kept_entry = bytes(v.image[keep:keep + 39])
        old_block = int.from_bytes(v.image[deleted + 17:deleted + 19], "little")
        v.image[deleted] = 0
        v.image[6 * 512 + old_block // 8] |= 0x80 >> (old_block % 8)
        struct.pack_into("<H", v.image, 2 * 512 + 4 + 33, 1)
        v.verify()
        v.add("IMPORT.A2C", b"A2C1\n0,0:42\n", 4)
        self.assertEqual(v.read("KEEP"), b"important user data")
        self.assertEqual(bytes(v.image[keep:keep + 39]), kept_entry)
        self.assertEqual(dict((name, off) for name, _, off in v.entries())["IMPORT.A2C"], deleted)
        v.verify()

    def test_full_directory_is_not_overwritten(self):
        v = Volume.format("DATA")
        for i in range(51):
            v.add("FILE" + str(i), b"", 4)
        before = bytes(v.image)
        with self.assertRaisesRegex(ValueError, "directory full"):
            v.add("EXTRA", b"oops", 4)
        self.assertEqual(bytes(v.image), before)

    def test_cli_safe_add_and_extract(self):
        tool = str(Path(__file__).resolve().parent.parent / "tools/prodos.py")
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / "DATA.dsk"
            original = Volume.format("DATA")
            original.add("KEEP", b"important", 4)
            source.write_bytes(to_dos(bytes(original.image)))
            before = source.read_bytes()
            local = directory / "IMPORT.A2C"
            local.write_bytes(b"A2C1\n0,0:42\n")
            output = directory / "NEWDATA.dsk"
            command = [sys.executable, tool, str(source), "--add", str(local),
                       "--name", "IMPORT.A2C", "--output", str(output)]
            run = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(source.read_bytes(), before)
            new = Volume(from_dos(output.read_bytes()))
            self.assertEqual(new.read("KEEP"), b"important")
            self.assertEqual(new.read("IMPORT.A2C"), local.read_bytes())
            self.assertEqual(new.image[2 * 512 + 5:2 * 512 + 9], b"DATA")
            second = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(second.returncode, 0)
            self.assertIn("already exists", second.stderr)
            self.assertEqual(new.image, Volume(from_dos(output.read_bytes())).image)
            same = command[:-1] + [str(source)]
            self.assertNotEqual(subprocess.run(same, capture_output=True).returncode, 0)
            self.assertEqual(source.read_bytes(), before)
            duplicate = [sys.executable, tool, str(output), "--add", str(local),
                         "--name", "IMPORT.A2C", "--output", str(directory / "OTHER.dsk")]
            self.assertNotEqual(subprocess.run(duplicate, capture_output=True).returncode, 0)
            self.assertFalse((directory / "OTHER.dsk").exists())
            extracted = directory / "EXTRACTED.A2C"
            get = [sys.executable, tool, str(output), "--extract", "IMPORT.A2C",
                   "--output", str(extracted)]
            self.assertEqual(subprocess.run(get, capture_output=True).returncode, 0)
            self.assertEqual(extracted.read_bytes(), local.read_bytes())
            self.assertNotEqual(subprocess.run(get, capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
