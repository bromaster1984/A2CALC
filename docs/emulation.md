# Headless boot validation

`tools/emulator-smoke.sh` runs the real 6502 program from the DOS-sector-order
140 KB disk image through the Disk II controller, ProDOS boot blocks, and an
Apple IIe enhanced 128 KB emulation. It does not inject the program into RAM.
It records the text screen in `build/emulator-boot.txt` and fails unless the
application's A2CALC title appears. No Apple ROM is redistributed in this
repository: the script obtains the emulator from its upstream repository.

Prerequisites: Node.js 20+, npm, Git, outbound HTTPS access to GitHub and npm.

```sh
./tools/emulator-smoke.sh dist/A2CALC.dsk dist/A2DATA.dsk
```

The test downloads pinned apple2js commit
`ee0aed25f73c69d0245e86a2a5fccb3324c3056c`, its pinned CPU submodule
`dca469ab95e38a3fa1a63e95be2f65aa155dc86d`, and esbuild 0.28.2 into a cache
(default `/tmp/a2calc-emu-build`; override with `A2CALC_EMU_CACHE`). The ROM
comes from upstream apple2js. Rendering is replaced by direct text RAM
inspection; CPU, MMU, ROM, ProDOS and disk controller execute normally.

To send keyboard bytes after startup, set `A2CALC_KEYS` to hexadecimal codes
separated by commas. A `wait:N` token executes N additional CPU cycles before
the next key; use it after disk operations. `A2CALC_EXPECT` requires text to appear after those keys.
The resulting text is also recorded in `build/emulator-input.txt`.
`A2CALC_DATA_OUT=/path/result.dsk` saves the modified second-drive image,
allowing independent extraction and validation of files written by the Apple
program. The original image is never overwritten unless explicitly selected
as the output path.

The test can catch bad boot blocks, wrong sector ordering, startup/linker
failures, memory corruption, and keyboard/file workflow regressions. An IIe
emulator cannot establish IIc ROM compatibility or prove physical mouse,
disk-drive timing, or ADTPro transfer. Real Apple IIc validation remains a
separate hardware test.

## Acceptance evidence

The development session booted the final image in the emulator and checked:

* 80-column application startup from the actual floppy boot chain.
* Demo arithmetic: D4=25, D5=35, D7=60, D9=30.
* Keyboard edit B4=12 recalculated D4=30, D7=65 and D9=32.5.
* Native save to the second floppy, including replacement of an existing file,
  followed by independent extraction and successful reopening in the emulator.
* Native loading of 128 occupied cells spanning both memory regions. Cell Z100,
  containing `=SUM(A1:Z5)`, displayed 127, and the UI reported 128/128 cells.
* Native XLSX export to the second floppy. The extracted file passed ZIP CRC and
  XML parsing; openpyxl read the preserved SUM formula and cached values 60 and 30.
  This establishes a valid OOXML file; it does not replace a direct Excel/Sheets
  import test.

Run the complete automated acceptance workflow (save/export, native reopening,
and full capacity):

```sh
python3 tools/emulator-validation.py
```

It writes diagnostic screens, an exported XLSX, and temporary data disk images
under `build/`, leaving the deliverable disks unchanged. A smaller repeatable
demo/save/export invocation is:

```sh
A2CALC_KEYS=04,13,0d,59,wait:30000000,18,0d \
A2CALC_EXPECT='XLSX exported' \
A2CALC_DATA_OUT=build/emulator-result.dsk \
./tools/emulator-smoke.sh dist/A2CALC.dsk dist/A2DATA.dsk
python3 tools/prodos.py build/emulator-result.dsk \
  --extract SHEET.XLSX --output build/emulator-result.xlsx
python3 -m zipfile -t build/emulator-result.xlsx
```

The initial `59` answers Y to replacement of the supplied empty SHEET.A2C.
The uncompressed XLSX exporter took roughly 69 million total emulated CPU cycles
in an isolated demo/export run (including boot and keyboard delays); allow a
minute or more on a real 1 MHz machine. Emulation wall time is unrelated to real
hardware runtime. Input loops wait for the keyboard strobe, and final expectations
allow up to 150 million further cycles for disk work.
