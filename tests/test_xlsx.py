"""Inspect files emitted by test_storage.c; no network or Excel dependency."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET
import zipfile

BASE = Path('/tmp/a2calc-storage-tests')


class XlsxExportTests(unittest.TestCase):
    def test_package_checksums_and_xml(self):
        for name in ('TEST.XLSX', 'FULL.XLSX'):
            with zipfile.ZipFile(BASE / name) as package:
                self.assertIsNone(package.testzip())
                self.assertEqual(len(package.namelist()), 5)
                for info in package.infolist():
                    self.assertEqual(info.compress_type, zipfile.ZIP_STORED)
                    self.assertTrue(info.flag_bits & 8)
                    ET.fromstring(package.read(info.filename))

    def test_independent_xlsx_reader(self):
        try:
            import openpyxl
        except ImportError:
            self.skipTest('openpyxl is not installed')
        formula = openpyxl.load_workbook(BASE / 'TEST.XLSX', data_only=False).active
        value = openpyxl.load_workbook(BASE / 'TEST.XLSX', data_only=True).active
        self.assertEqual(formula['A1'].value, 'Item & <price> "quoted"')
        self.assertEqual(formula['B3'].value, '=SUM(B1:B2)')
        self.assertEqual(formula['C3'].value, '=IF(B3>0,B3*2,0)')
        self.assertEqual(formula['Z100'].value, 'literal')
        self.assertEqual(value['B3'].value, 10.25)
        self.assertEqual(value['C3'].value, 20.5)
        self.assertEqual(value['D3'].value, '#DIV/0!')
        full = openpyxl.load_workbook(BASE / 'FULL.XLSX').active
        self.assertEqual(sum(1 for row in full for cell in row
                             if cell.value is not None), 128)


if __name__ == '__main__':
    unittest.main()
