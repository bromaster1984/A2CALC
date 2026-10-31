# A2CALC — instrukcja

## Dyskietki i start

Wymagany jest Apple IIc ze standardowymi 128 KB RAM, czytelny ekran 80 kolumn
oraz wewnętrzny i zewnętrzny napęd 5,25 cala. Kod używa 65C02; nie jest
przeznaczony dla oryginalnego Apple II/II+ z 6502 i ekranem 40 kolumn.

1. Uruchom klienta ADTPro na Apple IIc i serwer na komputerze. Można użyć
   standardowego bootstrapu ADTPro przez port szeregowy IIc.
2. Prześlij `A2CALC.dsk` z komputera na **całą dyskietkę w napędzie 1**.
3. Prześlij `A2DATA.dsk` na **drugą dyskietkę, w napędzie 2**.
4. Dysk programu można zabezpieczyć przed zapisem; dysk danych musi pozwalać
   na zapis. Pozostaw je w odpowiednich napędach i zrestartuj komputer.

Transfer całego obrazu nadpisuje wcześniejszą zawartość wskazanej dyskietki.
Nie kopiuj obrazu jako zwykłego pliku ProDOS. Oba pliki mają po 143360 bajtów,
35 ścieżek i kolejność sektorów DOS. Samo przemianowanie na `.po` jest błędne:
`.po` oznacza inną kolejność sektorów.

Wolne miejsce na dysku programu jest celowe: nie musi zajmować całych 140 KiB.
Dysk danych ma nazwę woluminu **DATA**, dysk programu **A2CALC**.

## Ekran i klawisze

Siatka pokazuje wyniki; pasek pod nią pełną treść bieżącej komórki/formuły.
`UNSAVED` oznacza niezapisane zmiany. Licznik mówi, ile spośród 128 dostępnych
komórek jest wypełnionych.

| Klawisz | Działanie |
|---|---|
| Strzałki | Ruch i automatyczne przewijanie |
| Tab | Następna kolumna |
| Return | Edycja bieżącej treści |
| Znak drukowalny | Zastąpienie treści, rozpoczęcie wpisywania |
| Delete poza edytorem | Usunięcie bieżącej komórki |
| Ctrl-C / Ctrl-V | Kopiowanie / wklejanie jednej komórki |
| Ctrl-Z | Cofnięcie ostatniej edycji jednej komórki |
| Ctrl-G | Skok do adresu, np. Z100 |
| Ctrl-F | Pierwsza komórka zawierająca szukany tekst |
| Ctrl-W | Szerokość kolumn widoku: 12, 18 lub 25 znaków |
| Ctrl-N / Ctrl-D | Nowy pusty arkusz / przykład |
| Ctrl-O / Ctrl-S | Otwarcie / zapis natywnego A2C |
| Ctrl-X | Eksport XLSX |
| Ctrl-L | Pomoc |
| Ctrl-Q | Wyjście do ProDOS |

Ctrl to przytrzymanie Control podczas naciśnięcia litery. Pomoc używa Ctrl-L,
ponieważ Ctrl-H ma na Apple II ten sam kod co strzałka w lewo.

W edytorze Return zatwierdza, Escape anuluje, Delete usuwa znak przed kursorem.
Strzałki lewo/prawo przesuwają kursor; Ctrl-A/Ctrl-E: początek/koniec;
Ctrl-K: wyczyszczenie pola. To samo dotyczy pól ścieżek plików.

Nie ma edycji zakresów ani wielopoziomowego cofania. Kopiowanie zapamiętuje
treść z chwili Ctrl-C; późniejsza edycja źródła nie zmienia schowka.
Za szeroka liczba jest oznaczana `###########`: poszerz widok Ctrl-W.
Tekst jest obcinany w siatce, lecz pełna treść pozostaje w komórce.
Szerokości są ustawieniem widoku, nie formatowaniem pliku.

## Mysz

Podłącz zgodną mysz Apple IIc do portu myszy/joysticka przed uruchomieniem.
Mysz szeregowa PC z DB9 nie jest zamiennikiem. Program używa firmware
AppleMouse i sterownika cc65; bez wykrytej myszy działa klawiatura.
Emulator musi udostępniać zgodne urządzenie AppleMouse, a nie tylko kursor hosta.

Kliknięcie komórki wybiera ją; ponowne kliknięcie wybranej rozpoczyna edycję.
Klikanie New, Open, Save, XLSX, Goto, Find, Help uruchamia polecenia.
Nie ma przeciągania zakresów i pasków przewijania — służą temu strzałki i Ctrl-G.
Fizyczne zachowanie myszy na IIc nie zostało sprawdzone.

## Dane i formuły

Siatka to **A1:Z100**, maksymalnie **128 niepustych komórek**, do **63 znaków
w każdej**. Tekst to drukowalne ASCII, bez polskich znaków. Apostrof wymusza
tekst: `'00123` lub `'=A1`. Liczby mają kropkę dziesiętną, bez separatorów
tysięcy, walut i zapisu wykładniczego.

Formuły zaczynają się `=`, mają angielskie nazwy i przecinki między argumentami:

```
=B4*C4
=SUM(D4:D10)
=IF(D11>100,D11*0.90,D11)
=ROUND(A1/3,2)
=AVERAGE(A1:B8)
=POWER(A1,2)
```

Operatory: `+ - * / ^ %`, nawiasy, porównania `= <> < <= > >=`.
Odwołania `A1`, `$A1`, `A$1`, `$A$1` są przesuwane podczas wklejania zgodnie
z blokadami. Wklejenie przesuwające odwołanie poza siatkę zgłasza błąd
i zachowuje komórkę docelową. Nie ma zakresów całych kolumn `A:A`, innych
zakładek, nazw zakresów i odwołań do zewnętrznych plików.

| Funkcje | Uwagi |
|---|---|
| SUM, AVERAGE, MIN, MAX, COUNT | Argumenty liczbowe i zakresy; tekst w zakresach jest pomijany |
| IF(warunek,tak,nie) | Wynik liczbowy; niewybrana gałąź nie jest obliczana |
| ABS, INT | Wartość bezwzględna; zaokrąglenie w dół |
| ROUND(liczba,miejsca) | Zaokrąglenie w ramach precyzji silnika |
| MOD(a,b) | Reszta zgodna ze znakiem dzielnika |
| AND, OR, NOT | Wyniki liczbowe 1 lub 0 |
| SQRT, POWER | Pierwiastek; potęga o całkowitym wykładniku |

Puste odwołanie ma wartość zero. Wykrywane są błędy składni, nieznane nazwy,
dzielenie przez zero, cykle, nieprawidłowe odwołania i przekroczenia zakresu.
Maksymalna głębokość formuł/zależności: 16.

Silnik używa arytmetyki stałoprzecinkowej: **±21474836.47**, zaokrąglenie
do setnych po każdym działaniu, remisy od zera. SQRT obcina wynik do setnych,
POWER nie obsługuje wykładników ułamkowych. Np. `=1/3*3` daje `0.99`; Excel
około `1`. Jawne ROUND należy stosować także w arkuszu współczesnym, jeśli
wymagana jest zgodność zaokrągleń pośrednich. SUM/AVERAGE mogą zgłosić
`#NUM!`, gdy pośrednia suma przekroczy zakres, nawet gdy średnia zmieściłaby się.

## Dwa napędy i zapis

Ścieżki odnoszą się do **nazw woluminów ProDOS**, nie numerów napędów.
`/DATA/SHEET.A2C` oznacza plik na dyskietce DATA, która powinna być w napędzie 2.
Dysk A2CALC pozostaje w napędzie 1. Nie wkładaj jednocześnie dwóch różnych
dysków o identycznej nazwie DATA.

Ctrl-S pyta o ścieżkę; Return przyjmuje domyślne `/DATA/SHEET.A2C`.
Jeśli plik istnieje, Y potwierdza zastąpienie. Inną nazwę wpiszesz po Ctrl-K,
np. `/DATA/BUDGET.A2C`. Nazwa pliku ProDOS ma do 15 znaków, zaczyna się literą
i zawiera A–Z, cyfry i kropki. Zachowaj `.A2C` dla dokumentów do ponownego
otwierania na Apple IIc. `.XLSX` jest eksportem, nie formatem natywnym.

Nie wyjmuj dysku podczas operacji. Zapis używa `A2CALC.TMP` i tymczasowej
kopii starego pliku `A2CALC.BAK`. Zastane TMP/BAK nie są nadpisywane; po
przerwaniu zasilania odczytaj je na komputerze i zachowaj kopie przed
porządkowaniem. Musi być miejsce na stary i nowy dokument jednocześnie.
Ładowanie najpierw sprawdza plik, potem ponownie otwiera go do odczytu.
Fizyczny błąd drugiego odczytu może pozostawić w pamięci niepełny dokument;
zachowuj kopie ważnych arkuszy.

## Excel i Google Sheets

Ctrl-X tworzy **rzeczywisty XLSX**, domyślnie `/DATA/SHEET.XLSX`: jeden arkusz,
teksty, liczby, formuły i ostatnie wyniki. To nieskompresowany pakiet ZIP/OOXML,
nie przemianowany CSV. Na procesorze 1 MHz eksport może trwać kilkadziesiąt
sekund lub dłużej. Poczekaj na ukończenie. Zapisz również wersję A2C.

Przeniesienie wyniku na komputer:

1. Zapisz dokument, zakończ program i uruchom ADTPro, np. bootstrapem szeregowym.
2. Odczytaj **dysk danych** przez ADTPro do nowego obrazu, np. `WORK.dsk`.
3. Wydobądź plik:

   ```sh
   python3 tools/prodos.py WORK.dsk --extract SHEET.XLSX --output SHEET.XLSX
   ```

4. Otwórz XLSX w Excelu lub zaimportuj do Google Sheets.

Samego obrazu `.dsk` nie otwiera się jako skoroszytu. Arkusz zgłasza potrzebę
przeliczenia po otwarciu; większa precyzja aplikacji docelowej może zmienić
wyniki. Testy sprawdzają ZIP/CRC/XML i odczyt openpyxl; rzeczywiste aplikacje
Excel i Sheets nie były dostępne do testu akceptacyjnego.

Import w drugą stronę odbywa się **na współczesnym komputerze**:

```sh
python3 tools/convert.py BOOK.xlsx IMPORT.A2C --sheet Sheet1
python3 tools/prodos.py dist/A2DATA.dsk --add IMPORT.A2C --name IMPORT.A2C --output NEWDATA.dsk
```

Nagraj NEWDATA.dsk przez ADTPro na nową dyskietkę danych i otwórz
`/DATA/IMPORT.A2C` przez Ctrl-O. Konwerter nie wymaga bibliotek poza Pythonem;
czyta również CSV. Odrzuca zbyt duże arkusze, daty i nieobsługiwane formuły.
Nie odtwarza formatowania. `--round` oraz `--transliterate` jawnie pozwalają
na utratę precyzji i zmianę znaków narodowych — domyślnie to błąd.
[Szczegóły konwertera](conversion.md).

## Status

Sprawdzono rozruch, edycję, przeliczanie, zapis, ponowny odczyt i eksport
w emulatorze Apple IIe enhanced 128 KB, także pełne 128 komórek. To nie
zastępuje testu fizycznego IIc, myszy, napędów i ADTPro. Jest to pierwsza
ograniczona implementacja, bez pełnej funkcjonalności współczesnych arkuszy.
