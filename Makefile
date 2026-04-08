CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -std=c11 -pedantic

PROGRAM = program
UNIT_TEST = tests/test_main

all: $(PROGRAM)

$(PROGRAM): src/main.c src/sql_engine.h
	$(CC) $(CFLAGS) -o $(PROGRAM) src/main.c

$(UNIT_TEST): tests/test_main.c src/main.c src/sql_engine.h
	$(CC) $(CFLAGS) -DUNIT_TEST -o $(UNIT_TEST) tests/test_main.c src/main.c

test: $(PROGRAM) $(UNIT_TEST)
	./$(UNIT_TEST)
	sh ./tests/integration.sh

clean:
	rm -f $(PROGRAM) $(UNIT_TEST)

.PHONY: all test clean
