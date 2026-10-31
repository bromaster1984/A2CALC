#include <assert.h>
#include <stdio.h>
#include <string.h>
#if !defined(__CC65__) && defined(__unix__)
#include <sys/resource.h>
#include <signal.h>
#endif
#include "sheet.h"
#include "storage.h"

static void writefile(const char *path, const char *s)
{
    FILE *f = fopen(path, "wb");
    assert(f); assert(fputs(s, f) >= 0); assert(fclose(f) == 0);
}

int main(void)
{
    unsigned char i;
    char s[64];
#if !defined(__CC65__) && defined(__unix__)
    struct rlimit before, limited;
    FILE *f;
#endif
    sheet_reset();
    assert(sheet_set(0, 0, "Item & <price> \"quoted\"") == 0);
    assert(sheet_set(1, 0, "12.50") == 0);
    assert(sheet_set(1, 1, "-2.25") == 0);
    assert(sheet_set(1, 2, "=SUM(B1:B2)") == 0);
    assert(sheet_set(2, 2, "=IF(B3>0,B3*2,0)") == 0);
    assert(sheet_set(25, 99, "'literal") == 0);
    assert(sheet_set(3, 2, "=1/0") == 0);
    sheet_recalc();
    assert(storage_save("/tmp/a2calc-storage-tests/TEST.A2C") == 0);
    /* Replacement retains a working target and removes successful backups. */
    assert(storage_save("/tmp/a2calc-storage-tests/TEST.A2C") == 0);
    sheet_reset();
    assert(storage_load("/tmp/a2calc-storage-tests/TEST.A2C") == 0);
    assert(sheet_count() == 7);
    assert(!strcmp(sheet_get(0, 0), "Item & <price> \"quoted\""));
    assert(!strcmp(sheet_get(25, 99), "'literal"));
    assert(sheet_value(1, 2) == 1025);
    assert(storage_xlsx("/tmp/a2calc-storage-tests/TEST.XLSX") == 0);
    assert(storage_xlsx("/tmp/a2calc-storage-tests/TEST.XLSX") == 0);
    writefile("/tmp/a2calc-storage-tests/BAD.A2C", "A2C1\n0,0:OK\n26,0:BAD\n");
    assert(storage_load("/tmp/a2calc-storage-tests/BAD.A2C") == STORAGE_FORMAT);
    assert(sheet_count() == 7);
    writefile("/tmp/a2calc-storage-tests/BAD.A2C", "A2C1\n0,0:OK\n0,0:DUP\n");
    assert(storage_load("/tmp/a2calc-storage-tests/BAD.A2C") == STORAGE_FORMAT);
    writefile("/tmp/a2calc-storage-tests/A2CALC.TMP", "user file");
    assert(storage_save("/tmp/a2calc-storage-tests/TEST.A2C") == STORAGE_BUSY);
    remove("/tmp/a2calc-storage-tests/A2CALC.TMP");
    writefile("/tmp/a2calc-storage-tests/A2CALC.BAK", "user backup");
    assert(storage_save("/tmp/a2calc-storage-tests/TEST.A2C") == STORAGE_BUSY);
    remove("/tmp/a2calc-storage-tests/A2CALC.BAK");
    assert(storage_save("/tmp/a2calc-storage-tests/A2CALC.TMP") == STORAGE_BUSY);
    assert(storage_save("/tmp/a2calc-storage-tests/A2CALC.BAK") == STORAGE_BUSY);
    /* A short write detected by fclose must preserve an earlier target. */
#if !defined(__CC65__) && defined(__unix__)
    writefile("/tmp/a2calc-storage-tests/KEEP.A2C", "original document\n");
    assert(getrlimit(RLIMIT_FSIZE, &before) == 0);
    limited = before; limited.rlim_cur = 32;
    signal(SIGXFSZ, SIG_IGN);
    assert(setrlimit(RLIMIT_FSIZE, &limited) == 0);
    assert(storage_save("/tmp/a2calc-storage-tests/KEEP.A2C") == STORAGE_IO);
    assert(setrlimit(RLIMIT_FSIZE, &before) == 0);
    f = fopen("/tmp/a2calc-storage-tests/KEEP.A2C", "rb"); assert(f);
    assert(fgets(s, sizeof(s), f)); assert(!strcmp(s, "original document\n"));
    fclose(f);
#endif
    sheet_reset();
    for (i = 0; i < SHEET_MAX_CELLS; ++i) {
        sprintf(s, "%u", (unsigned)i);
        assert(sheet_set(i % 26, i / 26, s) == 0);
    }
    assert(storage_save("/tmp/a2calc-storage-tests/FULL.A2C") == 0);
    sheet_reset();
    assert(storage_load("/tmp/a2calc-storage-tests/FULL.A2C") == 0);
    assert(sheet_count() == SHEET_MAX_CELLS);
    assert(sheet_value(23, 4) == 12700);
    assert(storage_xlsx("/tmp/a2calc-storage-tests/FULL.XLSX") == 0);
    puts("storage: native roundtrip, malformed input, safe replacement, XLSX export passed");
    return 0;
}
