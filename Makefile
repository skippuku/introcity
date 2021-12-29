
CC = gcc -std=gnu99

default: db_test

store.h.intro: store.h db_intro
	./db_intro store.h

db_intro: intro.c intro.h lexer.c pre.c stb_ds.h
	$(CC) intro.c -Wall -g -o $@

db_test: test.c store.h.intro intro.h basic.h
	$(CC) test.c -g -o $@
