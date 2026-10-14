CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
SRC = src/lexer.c src/parser.c src/value.c src/interp.c src/main.c

jws: $(SRC) src/jws.h
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm -lpthread

test: jws
	./tests/run.sh

clean:
	rm -f jws

.PHONY: test clean
