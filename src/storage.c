/* A2CALC persistence and streaming, uncompressed Office Open XML export.
 * C89 / cc65. No allocation, compression library or filesystem dependencies.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sheet.h"
#include "storage.h"

static FILE *out;
static uint32_t outpos, crc, partsize;
static unsigned char inpart;
static unsigned char used[(26 * 100 + 7) / 8];
static char line[80];

/* Fixed scratch names, in the destination directory. Existing scratch or
 * backup files are never removed or overwritten. */
static char scratch[80], backup[80];

/* fgets cannot distinguish an embedded NUL from a terminator. Read explicitly
 * so damaged documents cannot pass validation with silently truncated cells. */
static int read_line(FILE *f)
{
    unsigned n;
    int ch;
    n = 0;
    while ((ch = fgetc(f)) != EOF) {
        if (n >= sizeof(line) - 1 ||
            (ch != '\r' && ch != '\n' && (ch < 32 || ch > 126))) return -1;
        line[n++] = (char)ch;
        if (ch == '\n') break;
    }
    line[n] = 0;
    if (ferror(f)) return -1;
    return n ? 1 : 0;
}

static unsigned char exists(const char *p)
{
    FILE *f;
    f = fopen(p, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static unsigned char begin_write(const char *path)
{
    unsigned n;
    const char *slash;
    slash = strrchr(path, '/');
    n = slash ? (unsigned)(slash - path + 1) : 0;
    if (n + 11 >= sizeof(scratch)) return STORAGE_IO;
    memcpy(scratch, path, n);
    memcpy(backup, path, n);
    strcpy(scratch + n, "A2CALC.TMP");
    strcpy(backup + n, "A2CALC.BAK");
    if (!strcmp(path, scratch) || !strcmp(path, backup) ||
        exists(scratch) || exists(backup)) return STORAGE_BUSY;
    out = fopen(scratch, "wb");
    return out ? STORAGE_OK : STORAGE_IO;
}

static unsigned char finish_write(const char *path)
{
    unsigned char bad, hadfile;
    bad = (unsigned char)!!ferror(out);
    if (fclose(out)) bad = 1;
    if (bad) { remove(scratch); return STORAGE_IO; }
    hadfile = exists(path);
    if (hadfile && rename(path, backup)) {
        remove(scratch); return STORAGE_IO;
    }
    if (rename(scratch, path)) {
        if (hadfile) rename(backup, path);
        return STORAGE_IO;
    }
    if (hadfile) remove(backup);
    return STORAGE_OK;
}

unsigned char storage_save(const char *path)
{
    unsigned char i, c, r, status;
    const char *s;
    status = begin_write(path);
    if (status) return status;
    fputs("A2C1\n", out);
    for (i = 0; i < sheet_count(); ++i) {
        s = sheet_at(i, &c, &r);
        fprintf(out, "%u,%u:%s\n", (unsigned)c, (unsigned)r, s);
    }
    return finish_write(path);
}

/* Parse one line. All coordinates and lengths are checked before mutation. */
static unsigned char parse_line(unsigned char *c, unsigned char *r,
                                char **raw)
{
    char *p, *end;
    unsigned a, b, n;
    p = line;
    if (*p < '0' || *p > '9') return STORAGE_FORMAT;
    a = 0;
    do {
        a = a * 10 + (unsigned char)(*p++ - '0');
        if (a >= SHEET_COLS) return STORAGE_FORMAT;
    } while (*p >= '0' && *p <= '9');
    if (*p++ != ',') return STORAGE_FORMAT;
    if (*p < '0' || *p > '9') return STORAGE_FORMAT;
    b = 0;
    do {
        b = b * 10 + (unsigned char)(*p++ - '0');
        if (b >= SHEET_ROWS) return STORAGE_FORMAT;
    } while (*p >= '0' && *p <= '9');
    if (*p++ != ':') return STORAGE_FORMAT;
    n = (unsigned)strlen(p);
    if (n && p[n - 1] == '\n') p[--n] = 0;
    if (n && p[n - 1] == '\r') p[--n] = 0;
    if (!n || n > SHEET_TEXT_MAX) return STORAGE_FORMAT;
    for (end = p; *end; ++end)
        if ((unsigned char)*end < 32 || (unsigned char)*end > 126)
            return STORAGE_FORMAT;
    *c = (unsigned char)a; *r = (unsigned char)b; *raw = p;
    return STORAGE_OK;
}

unsigned char storage_load(const char *path)
{
    FILE *f;
    unsigned char c, r, count, status;
    unsigned index;
    int got;
    char *raw;
    f = fopen(path, "rb");
    if (!f) return STORAGE_IO;
    status = STORAGE_FORMAT;
    if (read_line(f) != 1 ||
        (strcmp(line, "A2C1\n") && strcmp(line, "A2C1\r\n"))) goto done;
    memset(used, 0, sizeof(used));
    count = 0;
    while ((got = read_line(f)) == 1) {
        if (parse_line(&c, &r, &raw)) goto done;
        index = (unsigned)r * SHEET_COLS + c;
        if ((used[index >> 3] & (1 << (index & 7))) ||
            count >= SHEET_MAX_CELLS) goto done;
        used[index >> 3] |= (1 << (index & 7));
        ++count;
    }
    if (got < 0) goto done;
    /* Reopen after validation. ProDOS/cc65 marks and buffered EOF handling
     * differ across DOS versions; a fresh handle also releases its I/O buffer
     * before the next pass and avoids a seek dependency in this small loader.
     * Keep the same disk mounted for both passes. */
    if (fclose(f)) return STORAGE_IO;
    f = fopen(path, "rb");
    if (!f) return STORAGE_IO;
    if (read_line(f) != 1) { status = STORAGE_IO; goto done; }
    sheet_reset();
    while ((got = read_line(f)) == 1) {
        if (parse_line(&c, &r, &raw) || sheet_set(c, r, raw)) {
            status = STORAGE_IO; goto done;
        }
    }
    status = got < 0 ? STORAGE_IO : STORAGE_OK;
    sheet_recalc();
done:
    fclose(f);
    return status;
}

/* Nibble CRC table keeps the exporter small and reasonably fast on a 65C02. */
static const uint32_t crctab[16] = {
    0x00000000UL,0x1DB71064UL,0x3B6E20C8UL,0x26D930ACUL,
    0x76DC4190UL,0x6B6B51F4UL,0x4DB26158UL,0x5005713CUL,
    0xEDB88320UL,0xF00F9344UL,0xD6D6A3E8UL,0xCB61B38CUL,
    0x9B64C2B0UL,0x86D3D2D4UL,0xA00AE278UL,0xBDBDF21CUL
};

static void byte(unsigned char c)
{
    fputc(c, out); ++outpos;
    if (inpart) {
        crc ^= c;
        crc = (crc >> 4) ^ crctab[(unsigned char)crc & 15];
        crc = (crc >> 4) ^ crctab[(unsigned char)crc & 15];
        ++partsize;
    }
}

static void word(unsigned v)
{
    byte((unsigned char)v); byte((unsigned char)(v >> 8));
}

static void dword(uint32_t v)
{
    word((unsigned)v); word((unsigned)(v >> 16));
}

static void text(const char *s)
{
    while (*s) byte((unsigned char)*s++);
}

static void xml(const char *s)
{
    while (*s) {
        switch (*s) {
            case '&': text("&amp;"); break;
            case '<': text("&lt;"); break;
            case '>': text("&gt;"); break;
            case '"': text("&quot;"); break;
            default: byte((unsigned char)*s);
        }
        ++s;
    }
}

static const char *const names[5] = {
    "[Content_Types].xml", "_rels/.rels", "xl/workbook.xml",
    "xl/_rels/workbook.xml.rels", "xl/worksheets/sheet1.xml"
};
static uint32_t offsets[5], sizes[5], checksums[5];

static void start_part(unsigned char i)
{
    offsets[i] = outpos;
    dword(0x04034B50UL); word(20); word(8); word(0);
    word(0); word(0x21); dword(0); dword(0); dword(0);
    word((unsigned)strlen(names[i])); word(0); text(names[i]);
    crc = 0xFFFFFFFFUL; partsize = 0; inpart = 1;
}

static void end_part(unsigned char i)
{
    inpart = 0;
    checksums[i] = crc ^ 0xFFFFFFFFUL;
    sizes[i] = partsize;
    dword(0x08074B50UL); dword(checksums[i]);
    dword(partsize); dword(partsize);
}

static void number(SheetNum value)
{
    char s[18];
    sheet_format(value, s); text(s);
}

static void worksheet(void)
{
    unsigned char c, r, kind, err, any;
    char ref[5];
    const char *s;
    text("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
         "<dimension ref=\"A1:Z100\"/><sheetViews><sheetView workbookViewId=\"0\"/>"
         "</sheetViews><sheetFormatPr defaultColWidth=\"12\" defaultRowHeight=\"15\"/>"
         "<sheetData>");
    for (r = 0; r < SHEET_ROWS; ++r) {
        any = 0;
        for (c = 0; c < SHEET_COLS; ++c) {
            s = sheet_get(c, r);
            if (!*s) continue;
            if (!any) { text("<row>"); any = 1; }
            sprintf(ref, "%c%u", 'A' + c, (unsigned)r + 1);
            text("<c r=\""); text(ref); text("\"");
            kind = sheet_kind(c, r); err = sheet_error(c, r);
            if (kind == 2) {
                text(" t=\"inlineStr\"><is><t xml:space=\"preserve\">");
                if (*s == '\'') ++s;
                xml(s); text("</t></is></c>");
            } else {
                if (err) text(" t=\"e\"");
                text(">");
                if (kind == 3) {
                    text("<f>"); xml(s + 1); text("</f>");
                }
                text("<v>");
                if (err) {
                    s = sheet_error_text(err);
                    /* Excel has no #CYCLE! / #OVERFLOW! error codes. */
                    if (!strcmp(s, "#DIV/0!") || !strcmp(s, "#REF!") ||
                        !strcmp(s, "#NAME?") || !strcmp(s, "#NUM!") ||
                        !strcmp(s, "#VALUE!")) xml(s);
                    else text("#VALUE!");
                } else number(sheet_value(c, r));
                text("</v></c>");
            }
        }
        if (any) text("</row>");
    }
    text("</sheetData></worksheet>");
}

unsigned char storage_xlsx(const char *path)
{
    unsigned char i, status;
    uint32_t central, centralsize;
    status = begin_write(path);
    if (status) return status;
    sheet_recalc(); outpos = 0; inpart = 0;
    start_part(0);
    text("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
         "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
         "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
         "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>");
    text(
         "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
         "</Types>");
    end_part(0); start_part(1);
    text("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
         "</Relationships>");
    end_part(1); start_part(2);
    text("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
         "<bookViews><workbookView/></bookViews><sheets><sheet name=\"Sheet1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
         "<calcPr calcId=\"191029\" fullCalcOnLoad=\"1\" forceFullCalc=\"1\"/>"
         "</workbook>");
    end_part(2); start_part(3);
    text("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
         "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
         "</Relationships>");
    end_part(3); start_part(4); worksheet(); end_part(4);
    central = outpos;
    for (i = 0; i < 5; ++i) {
        dword(0x02014B50UL); word(20); word(20); word(8); word(0);
        word(0); word(0x21); dword(checksums[i]);
        dword(sizes[i]); dword(sizes[i]);
        word((unsigned)strlen(names[i])); word(0); word(0); word(0);
        word(0); dword(0); dword(offsets[i]); text(names[i]);
    }
    centralsize = outpos - central;
    dword(0x06054B50UL); word(0); word(0); word(5); word(5);
    dword(centralsize); dword(central); word(0);
    return finish_write(path);
}
