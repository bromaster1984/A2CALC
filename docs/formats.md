# File formats

The Apple II application saves native documents as `NAME.A2C` and directly
exports genuine `.XLSX` files. Both belong on the **data disk in drive 2**.
Use ProDOS filenames: uppercase ASCII, a letter first, at most 15 characters,
letters/digits/periods only. `/DATA/BUDGET.A2C` is an example full path.

## Native A2C

ASCII text, first line `A2C1`, followed by one nonempty cell per line:

```
A2C1
0,0:Apples
1,0:12.50
1,1:=B1*2
```

Coordinates are zero based: `0,0` is A1. Cell text follows the first colon
unchanged, so colons in a formula or label are allowed. Text is printable ASCII;
formulas begin with `=`. A leading apostrophe forces text. A file may contain
up to 128 unique nonempty cells in A1:Z100, each at most 63 bytes. LF and CRLF
line endings are accepted. Out-of-range/duplicate cells, excessive lengths and
invalid headers are rejected before replacing the in-memory document.

Loading validates the complete file and then reopens and applies it.
A physical disk/read error during this second pass can leave a partial document;
keep the previous saved document on disk. Keep the same disk mounted throughout
loading. There is no second 10 KB sheet buffer.

Save/export writes `A2CALC.TMP` in the destination directory, closes it, renames
an existing destination to `A2CALC.BAK`, renames the temporary file to the final
name, then removes the backup. An existing TMP or BAK file causes an error and
is preserved. After an interrupted save, inspect these recovery files before
removing them. There is no guarantee against a power failure between filesystem
operations. A normal write failure does not truncate the previous destination.

## XLSX export

The exporter writes the real ZIP/Office Open XML package, using ZIP method 0
(STORE), CRC-32 checksums, data descriptors, a central directory and five XML
parts. Compression is deliberately omitted to fit the Apple II CPU/RAM budget.
The workbook has one worksheet, `Sheet1`, inline text strings, numeric cell
values, preserved formula source, and cached evaluated results. It requests a
full recalculation when opened in a modern spreadsheet. Standard A1 references,
ranges and supported English function names retain their Excel meanings.

Text and formulas are XML escaped. Apple II error codes without an OOXML
equivalent are exported as `#VALUE!`. Unsupported formulas are not converted
into invented functions. The sheet's numeric engine uses two decimal places;
Excel/Sheets recalculates formulas at its own precision, so divisions and
rounding-sensitive expressions can produce different results after opening.
Use `ROUND(...,2)` explicitly when matching financial rounding is important.

The Apple II application imports A2C, not arbitrary compressed XLSX workbooks.
Use the desktop conversion utility to convert a supported portion of a modern
workbook back to A2C. Multi-sheet workbooks, charts, styles, Unicode and modern
Excel-specific features do not fit this native format.

Native tests check valid ZIP CRCs and parse the resulting XML. An installed
`openpyxl` additionally reads exported formulas, inline strings and cached values.
Real Microsoft Excel and Google Sheets are separate products and require their
own end-to-end acceptance tests.

The `tests/test_storage_sim.c` harness also exercises the cc65-compiled exporter
on the sim65 65C02 emulator, including 16-bit `unsigned` ZIP fields and 32-bit
CRC arithmetic. sim65 lacks rename/remove, so this harness stubs those
operations and leaves the completed ZIP at
`/tmp/a2calc-storage-sim/A2CALC.TMP`; it does not validate ProDOS disk operations.
