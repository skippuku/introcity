
CC = gcc -std=gnu99

default: db_test.exe

store.h.intro: store.h db_intro.exe
	./db_intro.exe store.h

db_intro.exe: intro.c intro.h lexer.c pre.c stb_ds.h
	$(CC) intro.c -Wall -g -o $@

db_test.exe: test.c store.h.intro intro.h basic.h
	$(CC) test.c -g -o $@
