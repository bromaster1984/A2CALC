#!/usr/bin/env python3
"""Create ADTPro-ready program/data floppy images from the cc65 executable."""
from pathlib import Path
import hashlib
import sys
from prodos import Volume, TEMPLATE_SHA256, applesingle_data, to_dos, from_dos


def main():
    root = Path(__file__).resolve().parent.parent
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "build/A2CALC.SYSTEM"
    template = (root / "tools/vendor/ProDOS_2_4_3.po").read_bytes()
    if hashlib.sha256(template).hexdigest() != TEMPLATE_SHA256:
        raise SystemExit("ProDOS template checksum mismatch")
    source = Volume(template)
    program = applesingle_data(executable.read_bytes())
    app = Volume.format("A2CALC", template[:1024])
    app.add("PRODOS", source.read("PRODOS"), 0xFF, 0x2000)
    app.add("A2CALC.SYSTEM", program, 0xFF, 0x2000)
    app.add("READ.ME", (
        "A2CALC - APPLE IIC / 128K / 80 COLUMNS\r"
        "Boot this disk in drive 1. Put A2DATA in drive 2.\r"
        "Default sheet: /DATA/SHEET.A2C. Demo: /DATA/DEMO.A2C.\r"
        "Use Ctrl-L inside the program for keyboard help.\r"
        "Please read the supplied Polish manual before using real data.\r"
    ).encode("ascii"), 0x04)
    data = Volume.format("DATA")
    demo = (
        "A2C1\n"
        "0,0:ITEM\n1,0:QTY\n2,0:PRICE\n3,0:TOTAL\n"
        "0,1:APPLE\n1,1:3\n2,1:2.50\n3,1:=B2*C2\n"
        "0,2:DISK\n1,2:2\n2,2:5.25\n3,2:=B3*C3\n"
        "0,4:SUM\n3,4:=SUM(D2:D3)\n"
    ).encode("ascii")
    data.add("DEMO.A2C", demo, 0x04)
    data.add("SHEET.A2C", b"A2C1\n", 0x04)
    out = root / "dist"
    out.mkdir(exist_ok=True)
    for name, volume in (("A2CALC", app), ("A2DATA", data)):
        free = volume.verify()
        raw = bytes(volume.image)
        dos = to_dos(raw)
        if from_dos(dos) != raw:
            raise SystemExit("sector-order roundtrip failed")
        path = out / (name + ".dsk")
        path.write_bytes(dos)
        print(f"{path.name}: 143360 bytes; {free * 512} bytes free; DOS sector order")
    (out / "SHA256SUMS").write_text("".join(
        hashlib.sha256((out / name).read_bytes()).hexdigest() + "  " + name + "\n"
        for name in ("A2CALC.dsk", "A2DATA.dsk")), encoding="ascii")


if __name__ == "__main__":
    main()
