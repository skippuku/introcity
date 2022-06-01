IMGUI_PATH := ../modules/imgui/
EXE := intro

ifeq (,$(OS))
  UNAME := $(shell uname -s)
  ifeq (Linux,$(UNAME))
    OS := Linux
  endif
endif

ifneq (0,$(shell id -u))
  GIT_VERSION := $(shell git describe --abbrev=7 --tags --dirty --always)
else
  GIT_VERSION := unknown
endif
CFLAGS += -Wall -DVERSION='"$(GIT_VERSION)"'

SRC := intro.c lib/introlib.c
MAGIC_DEFAULT_PROFILE := debug

define PROFILE.release
  CFLAGS += -O2
  LDFLAGS += -s
  MAGIC_TARGET := build
endef

PROFILE_FLAGS := -fprofile-instr-generate -fcoverage-mapping
define PROFILE.profile
  CFLAGS += -O2 $(PROFILE_FLAGS)
  LDFLAGS += $(PROFILE_FLAGS)
  MAGIC_TARGET := build
endef

define PROFILE.debug
  CFLAGS += -g -DDEBUG
  MAGIC_TARGET := build
endef

SANITIZE_FLAGS := -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
define PROFILE.sanitize
  $(PROFILE.debug)
  CFLAGS += $(SANITIZE_FLAGS)
  LDFLAGS += $(SANITIZE_FLAGS)
  MAGIC_TARGET := build
endef

define PROFILE.config
  $(PROFILE.release)
  PROFILEDIR := release
endef

define PROFILE.test
  MAGIC_NODEP := 1
endef

define PROFILE.install
  MAGIC_NODEP := 1
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

.PHONY: build test install clean cleanall config

build: $(OBJDIR)/$(EXE)
	@echo "Build complete for $(PROFILE)."

$(OBJDIR)/$(EXE): $(OBJDIR)/intro.o $(OBJDIR)/introlib.o
	$(CC) -o $@ $^ $(LDFLAGS)

test:
	@$(MAKE) --no-print-directory --directory=test/ run

config: $(EXE)
	./$(EXE) --gen-config --compiler $(CC) --file intro.cfg

install:
ifeq (Linux,$(OS))
	./scripts/install.sh
	@echo "install successful, enjoy!"
else
	@echo "install target is not supported for this OS."
endif

clean:
	rm -rf $(BUILDDIR)/*

cleanall: clean
	@$(MAKE) -C test/ clean
	rm -f $(EXE)
