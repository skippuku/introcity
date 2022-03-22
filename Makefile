CC = gcc -std=gnu99

default: test.h.intro

test.h.intro: db_intro test.h
	./db_intro test.h

db_intro: intro.c intro.h parse.c lexer.c pre.c attribute.c stb_ds.h
	$(CC) intro.c -Wall -g -fdiagnostics-color=always -o $@

db_test: test.c store.h.intro intro.h basic.h
	$(CC) test.c -g -o $@
