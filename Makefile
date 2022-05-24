IMGUI_PATH := ../modules/imgui/
EXE := intro

GIT_VERSION = $(shell git describe --abbrev=7 --tags --dirty --always)

CFLAGS += -Wall -DVERSION='"$(GIT_VERSION)"'

SRC := intro.c lib/introlib.c lib/intro_imgui.cpp
MAGIC_DEFAULT_PROFILE := debug

define PROFILE.release
  CFLAGS += -O2
  LDFLAGS += -s
  MAGIC_TARGET := build
endef

define PROFILE.profile
  CFLAGS += -g -O2
  MAGIC_TARGET := build
endef

define PROFILE.debug
  CFLAGS += -g -DDEBUG
  MAGIC_TARGET := build
endef

define PROFILE.sanitize
  $(PROFILE.debug)
  SANITIZE_FLAGS := -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
  CFLAGS += $(SANITIZE_FLAGS)
  LDFLAGS += $(SANITIZE_FLAGS)
  MAGIC_TARGET := build
endef

define PROFILE.test
  $(PROFILE.debug)
  PROFILEDIR := debug
endef

define PROFILE.install
  $(PROFILE.release)
  PROFILEDIR := release
endef

define PROFILE.clean
  MAGIC_NODEP := 1
endef

define PROFILE.cleanall
  $(PROFILE.clean)
endef

include magic.mk

CXXFLAGS := $(CFLAGS) -std=c++11 -I$(IMGUI_PATH)
CFLAGS += -std=gnu99

.PHONY: build test install clean cleanall

build: $(EXE)
	@echo "Build complete for $(PROFILE)."

$(EXE): $(OBJDIR)/intro.o $(OBJDIR)/introlib.o
	$(CC) -o $@ $^ $(LDFLAGS)

test: build
	@$(MAKE) --directory=test/ run
	./$(EXE) intro.c -o test/intro.c.intro

PREFIX = /usr/local

install: build
	mkdir -p $(PREFIX)/bin
	cp -f $(EXE) $(PREFIX)/bin/intro
	chmod 755 $(PREFIX)/bin/intro

clean:
	rm -rf $(BUILDDIR)/*

cleanall: clean
	@$(MAKE) -C test/ clean
	rm -f $(EXE)
