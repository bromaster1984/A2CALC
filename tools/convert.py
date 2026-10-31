#!/usr/bin/env python3
"""Convert a supported XLSX worksheet or CSV file to Apple II A2CALC A2C1."""
import argparse
import csv
from decimal import Decimal, InvalidOperation, ROUND_HALF_UP
import os
from pathlib import Path
import posixpath
import re
import sys
import tempfile
import unicodedata
import xml.etree.ElementTree as ET
import zipfile

NS = {"s": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
      "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
      "p": "http://schemas.openxmlformats.org/package/2006/relationships"}
FUNCTIONS = {"SUM", "AVERAGE", "MIN", "MAX", "COUNT", "IF", "ABS", "ROUND",
             "INT", "MOD", "AND", "OR", "NOT", "SQRT", "POWER"}
AGGREGATES = {"SUM", "AVERAGE", "MIN", "MAX", "COUNT"}
LIMIT = Decimal("21474836.47")
REF = re.compile(r"\$?([A-Za-z]+)\$?([0-9]+)\Z")
NUM = re.compile(r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[Ee][+-]?[0-9]+)?\Z")
TOKEN = re.compile(r"\s+|\$?[A-Za-z]+\$?[0-9]+|[A-Za-z]+|(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)|<=|>=|<>|[+*/^%=<>()\-,:]")


class ConversionError(ValueError):
    pass


def coordinates(ref):
    match = REF.fullmatch(ref)
    if not match:
        raise ConversionError("invalid cell reference: " + ref)
    col = 0
    for char in match[1].upper():
        col = col * 26 + ord(char) - 64
    row = int(match[2])
    if not 1 <= col <= 26 or not 1 <= row <= 100:
        raise ConversionError("cell/reference outside A1:Z100: " + ref)
    return col - 1, row - 1


def numeric(value, rounding=False):
    if not NUM.fullmatch(value):
        raise ConversionError("invalid number: " + value)
    try:
        number = Decimal(value)
    except InvalidOperation as exc:
        raise ConversionError("invalid number: " + value) from exc
    if not number.is_finite() or abs(number) > LIMIT:
        raise ConversionError("number outside +/-21474836.47: " + value)
    rounded = number.quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)
    if number != rounded and not rounding:
        raise ConversionError("number needs more than two decimals (use --round explicitly): " + value)
    if not rounded:
        rounded = Decimal(0)
    return format(rounded, "f").rstrip("0").rstrip(".") if "." in format(rounded, "f") else str(rounded)


def ascii_text(value, transliterate=False):
    if transliterate:
        value = value.translate(str.maketrans({"ł": "l", "Ł": "L"}))
        value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode("ascii")
    if any(ord(c) < 32 or ord(c) > 126 for c in value):
        raise ConversionError("text must be printable ASCII (use --transliterate explicitly for accents)")
    return value


class Formula:
    """Syntax and subset validation; the Apple II engine performs evaluation."""
    def __init__(self, text, rounding=False):
        # Excel sometimes prefixes names even when they are standard functions.
        text = re.sub(r"_xlfn\.([A-Za-z]+)(?=\s*\()", r"\1", text, flags=re.I)
        self.tokens = []
        pos = 0
        while pos < len(text):
            match = TOKEN.match(text, pos)
            if not match:
                raise ConversionError("unsupported formula token near " + repr(text[pos:]))
            token = match[0]
            pos = match.end()
            if token.isspace():
                continue
            if token[0].isdigit() or token[0] == ".":
                token = numeric(token, rounding)
            self.tokens.append(token.upper())
        self.pos = 0
        self.depth = 0

    def peek(self):
        return self.tokens[self.pos] if self.pos < len(self.tokens) else ""

    def take(self):
        value = self.peek()
        self.pos += 1
        return value

    def expect(self, token):
        if self.take() != token:
            raise ConversionError("expected " + token + " in formula")

    def expression(self):
        self.sum()
        while self.peek() in ("=", "<", ">", "<=", ">=", "<>"):
            self.take()
            self.sum()

    def sum(self):
        self.term()
        while self.peek() in ("+", "-"):
            self.take()
            self.term()

    def term(self):
        self.exponent()
        while self.peek() in ("*", "/"):
            self.take()
            self.exponent()

    def exponent(self):
        self.unary()
        while self.peek() == "^":
            self.take()
            self.unary()

    def unary(self):
        while self.peek() in ("+", "-"):
            self.take()
        self.primary()
        while self.peek() == "%":
            self.take()

    def primary(self):
        token = self.take()
        if token == "(":
            self.nested()
            self.expression()
            self.expect(")")
            self.depth -= 1
        elif REF.fullmatch(token):
            coordinates(token)
        elif NUM.fullmatch(token) or token in ("TRUE", "FALSE"):
            return
        elif token in FUNCTIONS:
            self.expect("(")
            self.nested()
            count = 0
            if self.peek() != ")":
                while True:
                    # Ranges are valid only as entire aggregate arguments.
                    if (token in AGGREGATES and REF.fullmatch(self.peek())
                            and self.pos + 1 < len(self.tokens)
                            and self.tokens[self.pos + 1] == ":"):
                        coordinates(self.take())
                        self.take()
                        coordinates(self.take())
                    else:
                        self.expression()
                    count += 1
                    if self.peek() != ",":
                        break
                    self.take()
            self.expect(")")
            self.depth -= 1
            wanted = {"IF": 3, "ROUND": 2, "MOD": 2, "POWER": 2,
                      "ABS": 1, "INT": 1, "NOT": 1, "SQRT": 1}.get(token)
            if wanted is not None and count != wanted:
                raise ConversionError(token + " requires " + str(wanted) + " arguments")
            if token in ("AND", "OR") and not count:
                raise ConversionError(token + " requires arguments")
        else:
            raise ConversionError("unsupported function or invalid formula token: " + repr(token))

    def nested(self):
        self.depth += 1
        if self.depth >= 16:
            raise ConversionError("formula nesting exceeds Apple II engine limit")

    def validate(self):
        self.expression()
        if self.pos != len(self.tokens):
            raise ConversionError("unexpected formula token: " + self.peek())
        return "=" + "".join(self.tokens)


def cell_raw(value, kind, rounding=False, transliterate=False):
    if value is None or value == "":
        return ""
    if kind == "formula":
        value = Formula(value[1:] if value.startswith("=") else value, rounding).validate()
    elif kind == "number":
        value = numeric(value, rounding)
    elif kind == "boolean":
        if value not in ("0", "1"):
            raise ConversionError("invalid boolean value")
    else:
        value = "'" + ascii_text(value, transliterate)
    if len(value) > 63:
        raise ConversionError("cell exceeds 63 ASCII characters including text marker/formula equals sign")
    return value


def date_styles(archive):
    if "xl/styles.xml" not in archive.namelist():
        return set()
    root = xml(archive, "xl/styles.xml")
    date_ids = set(range(14, 23)) | set(range(27, 37)) | {45, 46, 47} | set(range(50, 59))
    for element in root.findall("s:numFmts/s:numFmt", NS):
        fmt = element.get("formatCode", "")
        fmt = re.sub(r'"[^"]*"|\\.|\[\$[^\]]*\]|\[(?:Red|Blue|Green|Black|White|Yellow|Cyan|Magenta|Color\d+)\]', "", fmt, flags=re.I)
        if re.search(r"[ymdhs]", fmt, re.I):
            date_ids.add(int(element.get("numFmtId")))
    return {i for i, el in enumerate(root.findall("s:cellXfs/s:xf", NS))
            if int(el.get("numFmtId", "0")) in date_ids}


def xml(archive, name):
    try:
        info = archive.getinfo(name)
        if info.file_size > 8 * 1024 * 1024:
            raise ConversionError("XLSX XML part exceeds 8 MB limit: " + name)
        return ET.fromstring(archive.read(name))
    except (KeyError, ET.ParseError) as exc:
        raise ConversionError("missing or invalid XLSX XML part: " + name) from exc


def read_xlsx(path, sheet=None, rounding=False, transliterate=False):
    cells = {}
    with zipfile.ZipFile(path) as archive:
        if sum(i.file_size for i in archive.infolist()) > 32 * 1024 * 1024:
            raise ConversionError("XLSX uncompressed size exceeds 32 MB safety limit")
        book = xml(archive, "xl/workbook.xml")
        sheets = book.findall("s:sheets/s:sheet", NS)
        selected = next((s for s in sheets if sheet is None or s.get("name") == sheet), None)
        if selected is None:
            raise ConversionError("worksheet not found: " + str(sheet))
        sheet_id = selected.get("{" + NS["r"] + "}id")
        rels = xml(archive, "xl/_rels/workbook.xml.rels")
        rel = next((r for r in rels if r.get("Id") == sheet_id), None)
        if rel is None or rel.get("TargetMode") == "External":
            raise ConversionError("missing or external worksheet relationship")
        target = rel.get("Target", "")
        name = posixpath.normpath(target.lstrip("/") if target.startswith("/") else "xl/" + target)
        if not name.startswith("xl/"):
            raise ConversionError("worksheet relationship escapes workbook")
        strings = []
        if "xl/sharedStrings.xml" in archive.namelist():
            for item in xml(archive, "xl/sharedStrings.xml").findall("s:si", NS):
                strings.append("".join(t.text or "" for t in item.iter("{" + NS["s"] + "}t")))
        dates = date_styles(archive)
        for cell in xml(archive, name).findall("s:sheetData/s:row/s:c", NS):
            ref = cell.get("r", "")
            formula = cell.find("s:f", NS)
            kind = cell.get("t", "n")
            value = cell.findtext("s:v", default="", namespaces=NS)
            if formula is not None:
                if formula.get("t", "normal") != "normal":
                    raise ConversionError(ref + ": shared/array/data-table formulas require explicit expansion first")
                value, kind = formula.text or "", "formula"
                if not value:
                    raise ConversionError(ref + ": empty formula")
            elif kind == "s":
                try:
                    index = int(value)
                    if index < 0:
                        raise IndexError
                    value = strings[index]
                except (IndexError, ValueError) as exc:
                    raise ConversionError(ref + ": invalid shared string") from exc
                kind = "text"
            elif kind == "inlineStr":
                value = "".join(t.text or "" for t in cell.findall("s:is//s:t", NS))
                kind = "text"
            elif kind == "b":
                kind = "boolean"
            elif kind == "n":
                kind = "number"
            elif kind == "str":
                kind = "text"
            elif kind in ("e", "d"):
                raise ConversionError(ref + ": error/date cells are unsupported")
            else:
                raise ConversionError(ref + ": unsupported cell type " + kind)
            if not value:
                continue
            if int(cell.get("s", "0")) in dates:
                raise ConversionError(ref + ": date/time formatting is unsupported; convert explicitly to text or plain numbers")
            put(cells, ref, value, kind, rounding, transliterate)
    return cells


def put(cells, ref, value, kind, rounding, transliterate):
    try:
        coord = coordinates(ref)
        raw = cell_raw(value, kind, rounding, transliterate)
        if coord in cells:
            raise ConversionError("duplicate cell")
        if raw:
            if len(cells) >= 128:
                raise ConversionError("more than 128 nonempty cells")
            cells[coord] = raw
    except ConversionError as exc:
        raise ConversionError(ref + ": " + str(exc)) from exc


def read_csv(path, rounding=False, transliterate=False, delimiter=","):
    cells = {}
    with open(path, "r", encoding="utf-8-sig", newline="") as stream:
        for row, values in enumerate(csv.reader(stream, delimiter=delimiter), 1):
            for col, value in enumerate(values):
                if not value:
                    continue
                if col >= 26 or row > 100:
                    raise ConversionError("nonempty CSV cell outside A1:Z100")
                kind = "formula" if value.startswith("=") else "number" if NUM.fullmatch(value) else "text"
                if value.startswith("'"):
                    value = value[1:]
                    kind = "text"
                put(cells, chr(65 + col) + str(row), value, kind, rounding, transliterate)
    return cells


def encode_a2c(cells):
    lines = ["A2C1"]
    for (col, row), value in sorted(cells.items(), key=lambda item: (item[0][1], item[0][0])):
        lines.append(str(col) + "," + str(row) + ":" + value)
    return ("\n".join(lines) + "\n").encode("ascii")


def write_output(path, data, force=False):
    path = Path(path)
    # Validate first, then use an exclusive creation or atomic replacement.
    if not force:
        with path.open("xb") as stream:
            stream.write(data)
        return
    fd, temporary = tempfile.mkstemp(prefix=".a2calc-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--sheet", help="XLSX worksheet name; default: first worksheet")
    parser.add_argument("--round", action="store_true", dest="rounding", help="explicitly round imported numbers/constants to two decimal places")
    parser.add_argument("--transliterate", action="store_true", help="explicitly remove non-ASCII accents/characters")
    parser.add_argument("--delimiter", default=",", help="CSV delimiter, e.g. ';'")
    parser.add_argument("--force", action="store_true", help="replace an existing output after full validation")
    args = parser.parse_args(argv)
    try:
        if args.input.resolve() == args.output.resolve():
            raise ConversionError("input and output must be different files")
        if len(args.delimiter) != 1:
            raise ConversionError("CSV delimiter must be one character")
        if args.input.suffix.lower() == ".xlsx":
            cells = read_xlsx(args.input, args.sheet, args.rounding, args.transliterate)
        elif args.input.suffix.lower() == ".csv":
            if args.sheet:
                raise ConversionError("--sheet applies only to XLSX")
            cells = read_csv(args.input, args.rounding, args.transliterate, args.delimiter)
        else:
            raise ConversionError("input must be .xlsx or .csv")
        write_output(args.output, encode_a2c(cells), args.force)
    except (ConversionError, OSError, zipfile.BadZipFile, UnicodeError, InvalidOperation, ValueError) as exc:
        print("Conversion failed: " + str(exc), file=sys.stderr)
        return 1
    print("Wrote " + str(len(cells)) + " cells to " + str(args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
