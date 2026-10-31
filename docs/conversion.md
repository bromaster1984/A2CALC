# Wymiana danych z Excelem i Google Sheets

Apple II eksportuje prawdziwy plik XLSX z wartościami i formułami. Kierunek
od współczesnego komputera do Apple II obsługuje narzędzie `tools/convert.py`.
Wymaga Python 3, bez dodatkowych pakietów. Rozpakowuje także zwykłe skompresowane
pliki XLSX. Google Sheets: najpierw pobierz arkusz jako Microsoft Excel `.xlsx`.

```
python3 tools/convert.py BUDZET.xlsx BUDZET.A2C --sheet Budzet
python3 tools/convert.py tabela.csv TABELA.A2C
python3 tools/convert.py tabela.csv TABELA.A2C --delimiter ';'
```

Domyślnie wybierany jest pierwszy arkusz. `--sheet` wybiera konkretny arkusz po
nazwie. Importuje wyłącznie jego niepuste komórki; pozostałe arkusze i formatowanie
nie są przenoszone. Każda nieobsługiwana komórka wybranego arkusza powoduje błąd
całej konwersji, z adresem komórki. Gotowy A2C trzeba następnie umieścić na
dyskietce danych ProDOS w drugim napędzie; sam plik A2C nie jest obrazem dysku.

Przykładowy pełny transfer na Apple II:

```
python3 tools/convert.py BUDZET.xlsx BUDZET.A2C
python3 tools/prodos.py dist/A2DATA.dsk --add BUDZET.A2C --name BUDZET.A2C --output BUDZET.dsk
```

Nagraj nowy obraz `BUDZET.dsk` przez ADTPro na dyskietkę danych, umieść ją w
drugim napędzie i otwórz `/DATA/BUDZET.A2C` w A2CALC. Docelowy obraz musi mieć
nową nazwę: narzędzie nie zastępuje istniejącego obrazu ani pliku na obrazie.
Plik XLSX wyeksportowany przez Apple II odzyskasz po odczytaniu dyskietki
danych przez ADTPro do nowego obrazu, np. `ODCZYT.dsk`:

```
python3 tools/prodos.py ODCZYT.dsk --extract BUDZET.XLSX --output BUDZET.XLSX
```

Dokładne ograniczenia narzędzia i porządek sektorów opisuje
[disk-tools.md](disk-tools.md).

Ograniczenia importu odpowiadają bieżącemu silnikowi: A1:Z100, najwyżej 128
niepustych komórek, do 63 znaków ASCII na komórkę (razem z `=` formuły lub
apostrofem oznaczającym tekst), liczby od -21474836.47 do 21474836.47,
dwa miejsca dziesiętne. Tekst z XLSX otrzymuje znacznik tekstowy, dlatego np.
tekst `=A1` lub `001` nie staje się przypadkiem formułą ani liczbą. W CSV
liczby są rozpoznawane automatycznie, a `=` świadomie oznacza formułę;
apostrof przed wartością wymusza tekst. Puste komórki są pomijane.

Obsługiwane są odwołania A1 z opcjonalnymi `$`, operatory `+ - * / ^ %`,
porównania `= <> < > <= >=`, nawiasy, stałe TRUE/FALSE oraz funkcje
SUM, AVERAGE, MIN, MAX, COUNT, IF, ABS, ROUND, INT, MOD, AND, OR, NOT, SQRT,
POWER. Zakresy można podawać jako argumenty pięciu funkcji agregujących.
Nazwy funkcji w XLSX są po angielsku. Notacja naukowa zwykłych wartości liczbowych
XLSX jest zamieniana na zapis dziesiętny; notacja naukowa wewnątrz formuł nie
jest obsługiwana. Łańcuchy tekstowe w formułach, odwołania do innych arkuszy
i plików, tablice dynamiczne, wspólne formuły XLSX, tabele danych, błędy
oraz rozpoznane formaty dat/czasu są odrzucane. Wspólne formuły trzeba wcześniej
rozwinąć do zwykłych formuł poszczególnych komórek.

Silnik Apple II zaokrągla działania pośrednie do setnych. Excel i Google Sheets
liczą z większą precyzją, więc zgodność zapisu formuł nie gwarantuje identycznego
wyniku obliczeń. POWER/`^` na Apple II przyjmuje całkowity wykładnik -31..31,
ROUND przyjmuje całkowitą liczbę miejsc -7..2; przekroczenie ograniczenia lub
zakresu liczbowego daje błąd silnika. SQRT daje wynik przybliżony do setnych.
Konwerter sprawdza składnię i dostępność funkcji, ale nie wykonuje formuł:
nie wykrywa z góry cykli, błędów obliczeń ani przekroczenia głębokości łańcucha
zależności. Takie błędy ujawni program Apple II.

Liczby i stałe formuł mające istotne cyfry poza dwoma miejscami są domyślnie
odrzucane. Świadomą utratę precyzji włącza `--round` (połówki od zera).
`--transliterate` świadomie usuwa diakrytykę, w tym polskie znaki, i inne
nieprzedstawialne znaki; bez tej opcji Unicode jest odrzucany. Operacja może
zmienić albo usunąć treść, dlatego sprawdź wynik przed pracą na Apple II.
Wartości logiczne XLSX są przenoszone jako liczby 1/0. Formatowanie, wykresy,
komentarze, scalania i szerokości kolumn nie są zachowywane.

Konwersja najpierw waliduje cały wybrany arkusz. Istniejący plik wynikowy nie
jest nadpisywany bez `--force`; przy tej opcji zastąpienie następuje po pełnej
walidacji przez plik tymczasowy. Źródło nigdy nie jest nadpisywane.

Testy: `python3 -m unittest discover -s tests -p 'test_convert.py'`.
Obejmują rzeczywiście skompresowany XLSX, wspólne i osadzone łańcuchy,
formuły, walidację ograniczeń, ochronę pliku wynikowego oraz wczytanie
wygenerowanego A2C przez natywnie skompilowany silnik C programu.
