all: interactive_test

interactive_test: interactive_test.c.intro interactive_test.c ../db_intro_lib.o
	$(CC) -g interactive_test.c ../db_intro_lib.o -o $@

interactive_test.c.intro: interactive_test.c
	../db_intro $^
