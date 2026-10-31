#!/usr/bin/env python3
"""Boot, operate, and inspect the real Apple-target binary. No hardware claims."""
from pathlib import Path
import io
import os
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

from prodos import Volume, from_dos, to_dos

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / 'build'


def run(name, keys, expected, data, output=None):
    env = dict(os.environ, A2CALC_KEYS=keys, A2CALC_EXPECT=expected)
    if output:
        env['A2CALC_DATA_OUT'] = str(output)
    result = subprocess.run(
        [str(ROOT / 'tools/emulator-smoke.sh'), str(ROOT / 'dist/A2CALC.dsk'), str(data)],
        cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (BUILD / (name + '.txt')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(name + ' failed; inspect build/' + name + '.txt')
    print('PASS', name)
    return result.stdout


def main():
    BUILD.mkdir(exist_ok=True)
    exported = BUILD / 'emulator-validation-data.dsk'
    run('emulator-save-export', '04,13,0d,59,wait:30000000,18,0d',
        'XLSX exported', ROOT / 'dist/A2DATA.dsk', exported)
    volume = Volume(from_dos(exported.read_bytes()))
    volume.verify()
    native = volume.read('SHEET.A2C')
    assert b'3,6:=SUM(D4:D5)\n' in native
    names = {name for name, _, _ in volume.entries()}
    assert 'A2CALC.TMP' not in names and 'A2CALC.BAK' not in names
    xlsx = volume.read('SHEET.XLSX')
    (BUILD / 'emulator-validation.xlsx').write_bytes(xlsx)
    with zipfile.ZipFile(io.BytesIO(xlsx)) as z:
        assert z.testzip() is None
        for name in z.namelist():
            ET.fromstring(z.read(name))
        ns = {'s': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
        sheet = ET.fromstring(z.read('xl/worksheets/sheet1.xml'))
        cells = {c.attrib['r']: c for c in sheet.findall('.//s:c', ns)}
        assert cells['D7'].find('s:f', ns).text == 'SUM(D4:D5)'
        assert cells['D7'].find('s:v', ns).text == '60'
        assert cells['D9'].find('s:v', ns).text == '30'
    print('PASS ProDOS allocation, native safe-save, XLSX CRC/XML/formulas/cache')
    run('emulator-native-reopen', '0f,0d', '|         60|', exported)

    lines = ['A2C1'] + [f'{i % 26},{i // 26}:1' for i in range(127)]
    lines.append('25,99:=SUM(A1:Z5)')
    full = Volume.format('DATA')
    full.add('SHEET.A2C', ('\n'.join(lines) + '\n').encode('ascii'), 4)
    full.verify()
    fullpath = BUILD / 'emulator-validation-full.dsk'
    fullpath.write_bytes(to_dos(bytes(full.image)))
    screen = run('emulator-full-capacity',
                 '0f,0d,wait:60000000,07,5a,31,30,30,0d',
                 '|        127|', fullpath)
    assert 'Z100 formula 128/128 cells' in screen
    print('PASS full-capacity low/high memory cell store, SUM over127numeric cells')


if __name__ == '__main__':
    try:
        main()
    except (AssertionError, RuntimeError, OSError) as error:
        print('FAIL:', error, file=sys.stderr)
        sys.exit(1)
