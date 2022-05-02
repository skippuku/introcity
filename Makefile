IMGUI_INCLUDE = -I../modules/imgui/
EXE = intro

GIT_VERSION = $(shell git describe --abbrev=7 --tags --dirty --always)

CFLAGS += -Wall -DVERSION='"$(GIT_VERSION)"' -fdiagnostics-color=always
CXXFLAGS += $(CFLAGS) -std=c++11 $(IMGUI_INCLUDE)
CFLAGS += -std=gnu99

SRC = intro.c lib/introlib.c lib/intro_imgui.cpp

.PHONY: release debug test install clean cleanall

all: debug

release: CFLAGS += -O2
release: LDFLAGS += -s
release: $(EXE)

debug: CFLAGS += -g
debug: $(EXE)

$(EXE): %: %.o lib/introlib.o
	$(CC) $(LDFLAGS) -o $@ $^

PREFIX = /usr/local

test: debug
	@$(MAKE) --directory=test/ run
	$(CPP) intro.c -D__INTRO__ | ./$(EXE) - -o test/intro.c.intro

install: release
	mkdir -p $(PREFIX)/bin
	cp -f $(EXE) $(PREFIX)/bin/intro
	chmod 755 $(PREFIX)/bin/intro

clean:
	rm -f lib/*.o *.o *.d

cleanall:
	@$(MAKE) --directory=test/ clean
	rm -f $(EXE)
	@$(MAKE) clean

define \n


endef

SRC_C = $(filter %.c,$(SRC))
SRC_CPP = $(filter %.cpp,$(SRC))

deps.d:
	> deps.d
	$(foreach f,$(SRC_C),$(CC) -MM -MT '$(f:.c=.o) deps.d' $(f) >> deps.d$(\n))
	$(foreach f,$(SRC_CPP),$(CC) -MM -MT '$(f:.cpp=.o) deps.d' $(f) >> deps.d$(\n))

ifneq ($(MAKECMDGOALS),clean)
include deps.d
endif
