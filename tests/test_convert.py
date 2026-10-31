import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("a2convert", ROOT / "tools" / "convert.py")
convert = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(convert)
S = convert.NS["s"]
R = convert.NS["r"]
P = convert.NS["p"]


def fixture(path, cells, shared="", styles="", other=""):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("xl/workbook.xml", '<workbook xmlns="' + S + '" xmlns:r="' + R + '"><sheets><sheet name="Budget" sheetId="1" r:id="rId1"/>' + ('<sheet name="Other" sheetId="2" r:id="rId2"/>' if other else '') + '</sheets></workbook>')
        archive.writestr("xl/_rels/workbook.xml.rels", '<Relationships xmlns="' + P + '"><Relationship Id="rId1" Target="worksheets/sheet1.xml"/>' + ('<Relationship Id="rId2" Target="/xl/worksheets/sheet2.xml"/>' if other else '') + '</Relationships>')
        archive.writestr("xl/worksheets/sheet1.xml", '<worksheet xmlns="' + S + '"><sheetData><row>' + cells + '</row></sheetData></worksheet>')
        if other:
            archive.writestr("xl/worksheets/sheet2.xml", '<worksheet xmlns="' + S + '"><sheetData><row>' + other + '</row></sheetData></worksheet>')
        if shared:
            archive.writestr("xl/sharedStrings.xml", '<sst xmlns="' + S + '">' + shared + '</sst>')
        if styles:
            archive.writestr("xl/styles.xml", '<styleSheet xmlns="' + S + '">' + styles + '</styleSheet>')


class ConversionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)
        self.xlsx = self.path / "book.xlsx"

    def tearDown(self):
        self.temp.cleanup()

    def test_deflated_xlsx_shared_inline_numbers_formula(self):
        fixture(self.xlsx, '<c r="A1" t="s"><v>0</v></c><c r="B1"><v>1.25E1</v></c><c r="B2"><v>3</v></c><c r="B3"><f>SUM(B1:B2)</f><v>15.5</v></c><c r="C1" t="inlineStr"><is><t>=literal</t></is></c>', '<si><r><t>App</t></r><r><t>les</t></r></si>')
        cells = convert.read_xlsx(self.xlsx)
        self.assertEqual(cells, {(0, 0): "'Apples", (1, 0): "12.5", (1, 1): "3", (1, 2): "=SUM(B1:B2)", (2, 0): "'=literal"})
        self.assertTrue(convert.encode_a2c(cells).startswith(b"A2C1\n0,0:'Apples\n"))

    def test_named_worksheet(self):
        fixture(self.xlsx, '<c r="A1"><v>1</v></c>', other='<c r="B2"><v>2</v></c>')
        self.assertEqual(convert.read_xlsx(self.xlsx, "Other"), {(1, 1): "2"})
        with self.assertRaises(convert.ConversionError):
            convert.read_xlsx(self.xlsx, "Missing")

    def test_formula_subset(self):
        formula = 'IF(SUM($A$1:B2)>=3,ROUND(POWER(2,3)/3,2),ABS(-1))'
        self.assertEqual(convert.Formula(formula).validate(), "=" + formula)
        self.assertEqual(convert.Formula('_xlfn.SUM(A1:A2)').validate(), '=SUM(A1:A2)')
        self.assertEqual(convert.Formula('and(TRUE,NOT(FALSE),10%)').validate(), '=AND(TRUE,NOT(FALSE),10%)')

    def test_unsupported_formulas_rejected(self):
        for formula in ('VLOOKUP(A1,A1:B2,2)', 'Sheet1!A1', '[book.xlsx]A1', 'SUM(AA1)', 'SUM(A101)', 'IF(1,"a","b")', 'A1&"x"', 'ABS(A1:A2)', 'SUM(A1:)', 'SUM()', 'ROUND(1)', '1+', 'A1 B1', '1E3'):
            if formula == 'SUM()':
                # Empty SUM is explicitly supported by the native engine.
                self.assertEqual(convert.Formula(formula).validate(), '=SUM()')
            else:
                with self.subTest(formula=formula), self.assertRaises(convert.ConversionError):
                    convert.Formula(formula).validate()
        with self.assertRaises(convert.ConversionError):
            convert.cell_raw('==1', 'formula')

    def test_precision_requires_explicit_rounding(self):
        for value, expected in [('1.235', '1.24'), ('-1.235', '-1.24'), ('0.001', '0')]:
            with self.assertRaises(convert.ConversionError):
                convert.numeric(value)
            self.assertEqual(convert.numeric(value, True), expected)
        self.assertEqual(convert.numeric('1.2300'), '1.23')
        with self.assertRaises(convert.ConversionError):
            convert.Formula('1.235+2').validate()
        self.assertEqual(convert.Formula('1.235+2', True).validate(), '=1.24+2')

    def test_numeric_limits(self):
        self.assertEqual(convert.numeric('-21474836.47'), '-21474836.47')
        for number in ('21474836.48', '-21474836.48', 'Infinity', 'NaN'):
            with self.assertRaises(convert.ConversionError):
                convert.numeric(number)

    def test_unicode_requires_explicit_transliteration(self):
        with self.assertRaises(convert.ConversionError):
            convert.cell_raw('Żółć', 'text')
        self.assertEqual(convert.cell_raw('Żółć', 'text', transliterate=True), "'Zolc")

    def test_dates_errors_shared_formulas_and_duplicates_rejected(self):
        examples = [
            ('<c r="A1" s="1"><v>45000</v></c>', '<cellXfs><xf numFmtId="0"/><xf numFmtId="14"/></cellXfs>'),
            ('<c r="A1" t="e"><v>#DIV/0!</v></c>', ''),
            ('<c r="A1"><f t="shared" si="0">B1</f><v>1</v></c>', ''),
            ('<c r="A1"><v>1</v></c><c r="A1"><v>2</v></c>', ''),
            ('<c r="A1" t="s"><v>-1</v></c>', ''),
        ]
        for cells, styles in examples:
            with self.subTest(cells=cells):
                fixture(self.xlsx, cells, shared='<si><t>text</t></si>', styles=styles)
                with self.assertRaises(convert.ConversionError):
                    convert.read_xlsx(self.xlsx)

    def test_capacity_and_length(self):
        cells = {}
        for i in range(128):
            convert.put(cells, chr(65 + i % 26) + str(i // 26 + 1), '1', 'number', False, False)
        with self.assertRaises(convert.ConversionError):
            convert.put(cells, 'Z100', '2', 'number', False, False)
        with self.assertRaises(convert.ConversionError):
            convert.cell_raw('x' * 63, 'text')
        self.assertEqual(len(convert.cell_raw('x' * 62, 'text')), 63)

    def test_csv_numbers_formulas_and_explicit_text(self):
        path = self.path / 'input.csv'
        path.write_text("Label;12.50;=B1*2;'001\n", encoding='utf-8')
        self.assertEqual(convert.read_csv(path, delimiter=';'), {(0, 0): "'Label", (1, 0): '12.5', (2, 0): '=B1*2', (3, 0): "'001"})

    def test_output_preserves_existing_and_input(self):
        output = self.path / 'out.a2c'
        output.write_bytes(b'original')
        with self.assertRaises(FileExistsError):
            convert.write_output(output, b'new')
        self.assertEqual(output.read_bytes(), b'original')
        convert.write_output(output, b'new', force=True)
        self.assertEqual(output.read_bytes(), b'new')

    def test_cli_validation_failure_does_not_replace_destination(self):
        fixture(self.xlsx, '<c r="AA1"><v>1</v></c>')
        output = self.path / 'out.a2c'
        output.write_bytes(b'original')
        self.assertEqual(convert.main([str(self.xlsx), str(output), '--force']), 1)
        self.assertEqual(output.read_bytes(), b'original')

    def test_generated_a2c_loads_with_native_engine(self):
        fixture(self.xlsx, '<c r="A1" t="inlineStr"><is><t>Apples</t></is></c><c r="B1"><v>12.5</v></c><c r="B2"><v>3</v></c><c r="B3"><f>SUM(B1:B2)</f><v>15.5</v></c>')
        data = self.path / 'test.a2c'
        data.write_bytes(convert.encode_a2c(convert.read_xlsx(self.xlsx)))
        source = self.path / 'load.c'
        source.write_text('#include <assert.h>\n#include <string.h>\n#include "sheet.h"\n#include "storage.h"\nint main(int n,char **v){assert(n==2);assert(storage_load(v[1])==0);assert(sheet_count()==4);assert(!strcmp(sheet_get(0,0),"\\\'Apples"));assert(!strcmp(sheet_get(1,2),"=SUM(B1:B2)"));assert(sheet_value(1,2)==1550);return 0;}\n')
        exe = self.path / 'load'
        subprocess.run(['cc', '-std=c89', '-I', str(ROOT / 'include'), str(source), str(ROOT / 'src/sheet.c'), str(ROOT / 'src/storage.c'), '-o', str(exe)], check=True, capture_output=True)
        subprocess.run([str(exe), str(data)], check=True)


if __name__ == '__main__':
    unittest.main()
