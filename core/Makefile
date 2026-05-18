CC=gcc
CFLAGS=-O2 -Wall -Wextra -Iinclude

SRC=src/alfred_correlator.c \
    src/alfred_tables.c \
    src/alfred_utils.c

DEMO=examples/main_demo.c

all:
	$(CC) $(CFLAGS) $(SRC) $(DEMO) -o alfred_demo

clean:
	rm -f alfred_demo
