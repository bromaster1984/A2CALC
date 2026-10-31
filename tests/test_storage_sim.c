/* cc65/sim65c02 arithmetic/ZIP export check. sim65 implements host open/write
 * but lacks filesystem rename/remove. The following stubs deliberately
 * retain the completed archive as A2CALC.TMP; this is NOT a ProDOS I/O test.
 */
#include <assert.h>
#include "sheet.h"
#include "storage.h"

unsigned char __fastcall__ _sysrename(const char *oldname, const char *newname)
{
    (void)oldname; (void)newname; return 0;
}
unsigned char __fastcall__ _sysremove(const char *name)
{
    (void)name; return 0;
}
int __fastcall__ _osmaperrno(unsigned char code)
{
    return code;
}
int main(void)
{
    sheet_reset();
    assert(sheet_set(0, 0, "Item & <price> \"quoted\"") == 0);
    assert(sheet_set(1, 0, "12.50") == 0);
    assert(sheet_set(1, 1, "-2.25") == 0);
    assert(sheet_set(1, 2, "=SUM(B1:B2)") == 0);
    assert(sheet_set(2, 2, "=IF(B3>0,B3*2,0)") == 0);
    assert(sheet_set(25, 99, "'literal") == 0);
    assert(sheet_set(3, 2, "=1/0") == 0);
    assert(storage_xlsx("/tmp/a2calc-storage-sim/TEST.XLSX") == 0);
    return 0;
}
