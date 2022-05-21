IMGUI_PATH := ../modules/imgui/
EXE = intro

GIT_VERSION = $(shell git describe --abbrev=7 --tags --dirty --always)

CFLAGS += -Wall -DVERSION='"$(GIT_VERSION)"'
CFLAGS += -MMD

SRC := intro.c lib/introlib.c lib/intro_imgui.cpp
OBJ := lib/introlib.o

ifeq (release,$(MAKECMDGOALS))
  CFLAGS += -O2
  LDFLAGS += -s
else ifeq (profile,$(MAKECMDGOALS))
  CFLAGS += -O2 -g
else
  ifeq (sanitize,$(MAKECMDGOALS))
    SANITIZE_FLAGS := -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
  endif
  CFLAGS += -g -DDEBUG $(SANITIZE_FLAGS)
  LDFLAGS += $(SANITIZE_FLAGS)
endif

CXXFLAGS := $(CFLAGS) -std=c++11 -I$(IMGUI_PATH)
CFLAGS += -std=gnu99

.PHONY: release debug test install clean cleanall sanitize

all: $(EXE)
release: $(EXE)
debug: $(EXE)
sanitize: $(EXE)
profile: $(EXE)

$(EXE): %: %.o $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

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
