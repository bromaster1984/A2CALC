# Mysz Apple IIc

Program używa statycznie dołączonego standardowego sterownika AppleMouse z cc65
(`a2e.stdmou.mou`), bez osobnego pliku sterownika na dyskietce. Sterownik rozpoznaje
sygnatury firmware urządzenia wskazującego w slotach 1–7; w Apple IIc interfejs
myszy jest udostępniony przez wewnętrzny firmware. Nie jest to sterownik myszy USB.
Potrzebna jest mysz zgodna z Apple IIc/AppleMouse i odpowiednie złącze komputera.

Pozycja 0–279 × 0–191 jest przeliczana na 80 × 24 znaki zgodnie z domyślnym
kursorem tekstowym cc65. Sterownik używa przerwań VBL i usług firmware
INITMOUSE/SETMOUSE/READMOUSE. Główna pętla odczytuje stan i wykrywa zbocze
naciśnięcia lewego przycisku. Przy odświeżaniu ekranu należy wywołać
`a2mouse_hide()`, a po nim `a2mouse_show()`; funkcje te są idempotentne.

Wymagany jest zwykły runtime cc65 dla ProDOS z obsługą przerwań. Nie należy
wyłączać przerwań ani budować tego programu jako samodzielnego programu ROM
bez runtime ProDOS. Gdy firmware myszy nie zostanie znaleziony, inicjalizacja
zwraca 0 i program może nadal działać z klawiaturą.

Kod integracyjny można skompilować dla Apple II, ale fizyczne przesuwanie kursora
i przycisk wymagają sprawdzenia w emulatorze z emulacją myszy Apple II lub na
prawdziwym Apple IIc. Sam udany build tego nie weryfikuje.

Źródła protokołu i implementacji używanej przez program:

- https://github.com/cc65/cc65/blob/V2.19/libsrc/apple2/mou/a2.stdmou.s
- https://github.com/cc65/cc65/blob/V2.19/libsrc/apple2/mcbdefault.s
- https://github.com/cc65/cc65/blob/V2.19/doc/apple2enh.sgml

Sterownik cc65 odwołuje się do Apple II Mouse TechNote #1,
„Interrupt Environment with the Mouse”.
