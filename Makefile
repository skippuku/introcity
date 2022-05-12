IMGUI_INCLUDE = -I../modules/imgui/
EXE = intro

GIT_VERSION = $(shell git describe --abbrev=7 --tags --dirty --always)

CFLAGS += -Wall -DVERSION='"$(GIT_VERSION)"'
CFLAGS += -MMD

ifeq (release,$(MAKECMDGOALS))
  CFLAGS += -O2
  LDFLAGS += -s
else
  CFLAGS += -g -DDEBUG
endif

CXXFLAGS := $(CFLAGS) -std=c++11 $(IMGUI_INCLUDE)
CFLAGS += -std=gnu99

SRC = intro.c lib/introlib.c lib/intro_imgui.cpp
OBJ := $(addsuffix .o,$(basename $(SRC)))

.PHONY: release debug test install clean cleanall

all: $(EXE)
release: $(EXE)
debug: $(EXE)

$(EXE): %: %.o lib/introlib.o
	$(CC) $(LDFLAGS) -o $@ $^

PREFIX = /usr/local

test: debug
	@$(MAKE) --directory=test/ run
	./$(EXE) intro.c -o test/intro.c.intro

install: release
	mkdir -p $(PREFIX)/bin
	cp -f $(EXE) $(PREFIX)/bin/intro
	chmod 755 $(PREFIX)/bin/intro

clean:
	rm -f lib/*.o *.o lib/*.d *.d

cleanall:
	@$(MAKE) --directory=test/ clean
	@$(MAKE) clean
	rm -f $(EXE)

DEPS := $(addsuffix .d,$(basename $(SRC)))

$(DEPS): %.d: %.o

ifeq (,$(filter clean cleanall,$(MAKECMDGOALS)))
include $(DEPS)
endif
