CC65_HOME ?= /workspace/cc65-toolchain
CL65 ?= $(CC65_HOME)/bin/cl65
HOST_CC ?= cc
PYTHON ?= python3
SOURCES = src/main.c src/sheet.c src/storage.c src/mouse.c src/memory.s
HEADERS = $(wildcard include/*.h)

.PHONY: all disk test clean
all: disk

build/A2CALC.SYSTEM: $(SOURCES) $(HEADERS) cfg/apple2c-system.cfg
	mkdir -p build
	$(CL65) -t apple2enh -C cfg/apple2c-system.cfg -O -I include -m build/A2CALC.map -Ln build/A2CALC.lbl -o $@ $(SOURCES)

disk: build/A2CALC.SYSTEM
	$(PYTHON) tools/build_disk.py $<

test:
	mkdir -p build
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I include src/sheet.c tests/test_sheet.c -o build/test_sheet
	build/test_sheet
	mkdir -p /tmp/a2calc-storage-tests
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I include src/sheet.c src/storage.c tests/test_storage.c -o build/test_storage
	build/test_storage
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -rf build dist src/*.o
