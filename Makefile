CFLAGS += -std=gnu99
SRC = intro.c lexer.c pre.c parse.c attribute.c gen.c
DIAG_COLOR = -fdiagnostics-color=always 

default: test/db_test
	./test/db_test

.PHONY: test
test: db_intro test/db_test test/db_city_test
	./test/db_test
	./test/db_city_test
	./db_intro intro.c -o test/intro.c.intro

db_intro: $(SRC) db_intro_lib.o
	$(CC) $(CFLAGS) $(DIAG_COLOR) intro.c db_intro_lib.o -Wall -g -o $@

r_intro: $(SRC) r_intro_lib.o
	$(CC) $(CFLAGS) $(DIAG_COLOR) intro.c r_intro_lib.o -Wall -O2 -s -o $@

LIB_SRC = lib/lib.c lib/city.c lib/intro.h lib/types.h lib/ext/stb_ds.h lib/ext/stb_sprintf.h

db_intro_lib.o: $(LIB_SRC)
	$(CC) -g -c lib/lib.c -o $@

r_intro_lib.o: $(LIB_SRC)
	$(CC) -O2 -c lib/lib.c -o $@

db_intro_imgui.o: $(LIB_SRC) lib/intro_imgui.cpp
	$(CXX) $(DIAG_COLOR) -c -g -I../modules/imgui/ lib/intro_imgui.cpp -o $@

test/test.h.intro: db_intro test/test.h lib/types.h
	./db_intro test/test.h

test/db_test: test/test.c test/test.h.intro test/basic.h db_intro_lib.o
	$(CC) test/test.c db_intro_lib.o -g -o $@

city: test/db_city_test
	./test/db_city_test
	xxd -c 16 -g 1 test/obj.cty

test/db_city_test: test/city_test.c test/test.h.intro db_intro_lib.o
	$(CC) test/city_test.c db_intro_lib.o -g -o $@

PREFIX = /usr/local

install: r_intro
	mkdir -p $(PREFIX)/bin
	cp -f r_intro $(PREFIX)/bin/intro
	chmod 755 $(PREFIX)/bin/intro
