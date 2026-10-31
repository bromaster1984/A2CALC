# Weryfikacja wydania

Sprawdzono 31 października 2026 r. Obrazy końcowe:

```
A2CALC.dsk  8e199717c1f00a90eca53dad08a41ed6910198657b4d90ac6d0045b09f881c58
A2DATA.dsk  e08df460fe9117373cc755b62ad56a842f57b9675c105ed5a62b9ebf43cf1f77
```

- `make test`: 633 sprawdzenia silnika C, testy zapisu/odczytu i 22 testy Python
  (konwerter, system plików ProDOS, ZIP/XML/XLSX). Bez błędów.
- Silnik skompilowany cc65 uruchomiono przez sim65: 633 sprawdzenia poprawne.
- Eksporter skompilowany dla 65C02 zapisuje poprawny ZIP, odczytywany przez
  openpyxl z tekstami, formułami, błędami i zapisanymi wynikami.
- `python3 tools/emulator-validation.py`: rozruch rzeczywistego obrazu przez
  Disk II/ProDOS; 80 kolumn; utworzenie przykładu; zapis z zastąpieniem pliku
  na napędzie 2; ponowne otwarcie; eksport XLSX; niezależne sprawdzenie plików.
- Pełne 128 komórek, zajmujące obie części pamięci: odczyt i przeliczanie;
  Z100 zawierające `=SUM(A1:Z5)` pokazuje 127.
- Eksport przykładu: zachowane `SUM(D4:D5)` z wynikiem 60, średnia 30;
  poprawne CRC ZIP, XML i odczyt openpyxl. Przykładowy XLSX znajduje się
  w pakiecie w `examples/DEMO.xlsx`.
- Oba obrazy mają 143360 bajtów. Sprawdzono przydział bloków, katalogi,
  odczyt plików i konwersję kolejności sektorów.

Wykryty podczas pracy błąd ENOMEM przy otwieraniu plików usunięto zmniejszeniem
kodu. Końcowa pamięć: komórki LOWBSS $0800–$1ECF, BSS $9E7E–$AFDA,
co zostawia 1829 bajtów sterty przed zarezerwowanym stosem $B700–$BEFF.
Zabezpieczenie linkera wymaga co najmniej 1536 bajtów na wyrównany bufor ProDOS.

Emulacja: Apple IIe enhanced 128 KB, 65C02; **nie fizyczny Apple IIc**.
Nie sprawdzono sprzętowej myszy IIc, fizycznych napędów, transferu ADTPro
ani rzeczywistego importu w interfejsie Excel/Google Sheets. Dostarczony
program ma opisane ograniczenia i nie jest pełnym odpowiednikiem tych aplikacji.
