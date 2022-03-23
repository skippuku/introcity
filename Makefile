CC = gcc -std=gnu99

default: test/db_test
	./test/db_test

db_intro: intro.c intro.h lexer.c pre.c parse.c attribute.c gen.c stb_ds.h
	$(CC) intro.c -Wall -g -fdiagnostics-color=always -o $@

test/test.h.intro: db_intro test/test.h
	./db_intro test/test.h

test/db_test: test/test.c test/test.h.intro intro.h test/basic.h
	$(CC) test/test.c -g -o $@
