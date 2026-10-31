# A2CALC

Samodzielny arkusz dla **Apple IIc, 128 KB, 80 kolumn, dwa napędy 5,25 cala**.
Uruchamia się z dyskietki ProDOS, obsługuje klawiaturę i zgodną mysz Apple,
eksportuje prawdziwe pliki **XLSX** z formułami.

## Obrazy i uruchomienie

- `dist/A2CALC.dsk` — bootowalny dysk programu; napęd wewnętrzny.
- `dist/A2DATA.dsk` — dysk danych, wolumin `DATA`; napęd zewnętrzny.
- `dist/SHA256SUMS` — sumy kontrolne.

Każdy obraz ma **143360 bajtów**, 35 ścieżek i kolejność sektorów DOS.
Nagraj oba jako całe dyskietki przez ADTPro, następnie zrestartuj IIc z A2CALC
w napędzie 1 i DATA w napędzie 2. `Ctrl-D`: przykład; `Ctrl-L`: pomoc.

[Instrukcja po polsku](docs/INSTRUKCJA.md) — klawisze, formuły, zapis,
mysz, wymiana plików z Excelem i Google Sheets.

## Funkcje i ograniczenia

Siatka **A1:Z100**, maksymalnie **128 niepustych komórek**, do **63 znaków**
na komórkę. Przewijanie, edycja, kopiowanie z przesuwaniem odwołań,
jednopoziomowe cofanie, wyszukiwanie, zmiana szerokości widoku, przeliczanie,
wykrywanie błędów i cykli. ASCII, bez polskich znaków.

Formuły: `+ - * / ^ %`, porównania, nawiasy, odwołania względne i `$A$1`,
zakresy; `SUM AVERAGE MIN MAX COUNT IF ABS ROUND INT MOD AND OR NOT SQRT POWER`.
POWER obsługuje całkowite wykładniki; wyniki funkcji są liczbowe.

Liczby mają **dwa miejsca po przecinku**, zakres ±21474836,47, zaokrąglenie
po każdym działaniu. Np. `=1/3*3` daje tutaj `0.99`, a Excel około `1`.
Maksymalna głębokość formuł i zależności to 16.

To pierwsza, ograniczona implementacja, **nie pełny odpowiednik Google Sheets
ani program wykorzystujący wszystkie możliwości IIc**. Nie ma wykresów, dat,
formatowania, wielu zakładek, makr, formuł tekstowych ani drukowania.
Nie wykorzystuje całej pamięci pomocniczej na przechowywanie arkusza.

## Wymiana XLSX

`Ctrl-X` zapisuje poprawny pakiet OOXML/ZIP `.XLSX`: teksty, liczby, źródła
formuł i ostatnie wyniki. To nie jest przemianowany CSV. Testy potwierdzają
odczyt przez openpyxl; nie wykonano testów w rzeczywistym Excelu/Sheets.
Wyższa precyzja ich silników może zmienić wyniki po przeliczeniu.

Apple IIc otwiera natywne `.A2C`. Import obsługiwanego fragmentu XLSX/CSV
wymaga konwertera na współczesnym komputerze:

```sh
python3 tools/convert.py workbook.xlsx IMPORT.A2C
python3 tools/prodos.py dist/A2DATA.dsk --add IMPORT.A2C --name IMPORT.A2C --output NEWDATA.dsk
```

Konwerter odrzuca nieobsługiwane funkcje i przekroczone limity; nie pomija
danych po cichu. [Szczegóły](docs/conversion.md).

## Budowanie i testy

Wymagane: cc65 V2.19, Make, Python 3.10+; kompilator C do testów natywnych.
Sprawdzony commit cc65: `555282497c3ecf8b313d87d5973093af19c35bd5`.

```sh
make CC65_HOME=/sciezka/do/cc65 disk
make test
```

Można również wskazać `CL65=/sciezka/do/cl65`. Budowanie nie wymaga usług
sieciowych: oficjalny szablon ProDOS 2.4.3 znajduje się w repozytorium,
z sumą SHA-256 i [pochodzeniem](tools/vendor/README.md).

Testy: silnik C i kod 6502, zapis/odczyt, struktura i CRC XLSX, konwersja,
przydział bloków dyskietek. [Test emulatora](docs/emulation.md) uruchamia
rzeczywisty obraz przez Disk II w Apple IIe enhanced 128 KB i sprawdza
operacje na drugim napędzie. Fizyczny IIc, mysz i ADTPro nie były testowane.

Biblioteka i sterownik myszy cc65 oraz ProDOS zachowują własne autorstwo
i warunki licencji. Nie są kodem własnym A2CALC.
