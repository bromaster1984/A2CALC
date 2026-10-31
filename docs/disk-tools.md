# Pliki na obrazie dyskietki

`tools/prodos.py` działa na współczesnym komputerze z Pythonem 3; nie wymaga
dodatkowych bibliotek. Obsługuje dostarczone obrazy 140 KB: `.dsk`/`.do`
w kolejności sektorów DOS oraz `.po` w kolejności bloków ProDOS.

Wyświetlenie katalogu:

```sh
python3 tools/prodos.py dist/A2DATA.dsk
```

Dodanie arkusza skonwertowanego narzędziem `tools/convert.py`:

```sh
python3 tools/prodos.py dist/A2DATA.dsk --add build/BUDZET.A2C --name BUDZET.A2C --output dist/BUDZET.dsk
```

Powstaje nowy obraz `dist/BUDZET.dsk` z dotychczasowymi plikami i nowym arkuszem.
Nazwa woluminu pozostaje `DATA`, więc po nagraniu tego obrazu przez ADTPro
i umieszczeniu dyskietki w drugim napędzie otwórz w A2CALC `/DATA/BUDZET.A2C`.
Nazwy plików ProDOS mają najwyżej 15 znaków, zaczynają się literą i zawierają
wielkie litery A–Z, cyfry oraz kropki. Dodawany plik otrzymuje typ tekstowy TXT.

Obraz wejściowy jest zachowany. Narzędzie wymaga nowej ścieżki wyjściowej,
odmawia nadpisania istniejącego pliku oraz dodania nazwy już obecnej w katalogu.
Przed zapisem sprawdza system plików i odczytuje ponownie zawartość wszystkich
dotychczasowych plików. Potrafi wykorzystać wolne wpisy pozostałe po usuwaniu
plików przez ProDOS. Obsługuje płaski katalog i pliki seedling/sapling; odmawia
modyfikacji układów, których nie potrafi bezpiecznie zweryfikować.

Wyjęcie arkusza lub pliku XLSX z obrazu odczytanego przez ADTPro:

```sh
python3 tools/prodos.py build/ZAPIS.dsk --extract BUDZET.A2C --output build/ODCZYT.A2C
python3 tools/prodos.py build/ZAPIS.dsk --extract BUDZET.XLSX --output build/BUDZET.xlsx
```

Każda ścieżka wyjściowa musi być nowa. Przed nagrywaniem dyskietki zawierającej
ważne dane odczytaj ją do osobnego obrazu przez ADTPro i zachowaj kopię.
