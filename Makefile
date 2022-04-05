CFLAGS += -std=gnu99
SRC = intro.c intro.h lexer.c pre.c parse.c attribute.c gen.c stb_ds.h stb_sprintf.h

default: test/db_test
	./test/db_test

test: db_intro test/db_test
	./test/db_test
	./db_intro intro.c -o test/intro.c.intro

db_intro: $(SRC)
	$(CC) $(CFLAGS) intro.c -Wall -g -fdiagnostics-color=always -o $@

r_intro: $(SRC)
	$(CC) $(CFLAGS) intro.c -Wall -O2 -s -o $@

test/test.h.intro: db_intro test/test.h
	./db_intro test/test.h

test/db_test: test/test.c test/test.h.intro intro.h test/basic.h
	$(CC) test/test.c -g -o $@
