INTRO_PARSE := ../build/debug/intro
CPPFLAGS += -I../lib
CFLAGS = -g -MMD
SRC := $(wildcard *.c)
INTERACTIVE := interactive_test
EXE := $(SRC:%.c=db_%)
TESTS := $(filter-out db_$(INTERACTIVE),$(EXE))
ICFG := ../intro.cfg

INTRO_SHARED := libintro.so

export ASAN_OPTIONS = detect_leaks=0

.PHONY: clean run
.PRECIOUS: %.intro

all: $(EXE)
	@echo "Build complete."

run: $(TESTS)
	@./../scripts/run_tests.sh $(TESTS)
	$(INTRO_PARSE) --cfg $(ICFG) ../intro.c -o intro.c.intro

$(INTRO_PARSE): FORCE
	@$(MAKE) -C .. debug

$(ICFG):
	@$(MAKE) -C .. config

%.intro: % $(INTRO_PARSE) FORCE
	$(INTRO_PARSE) --cfg $(ICFG) $(CPPFLAGS) $< -o $@

intro.h.intro: ../lib/intro.h $(INTRO_PARSE) FORCE
	$(INTRO_PARSE) --pragma "enable all" --cfg $(ICFG) $(CPPFLAGS) $< -o $@

FORCE:

$(EXE): db_%: %.c $(INTRO_SHARED)
	$(CC) $< -o $@ $(CFLAGS) $(CPPFLAGS) -L. -Wl,-rpath=. -l:$(INTRO_SHARED)

clean:
	rm -f *.intro *.o *.d $(EXE) $(INTRO_SHARED)

%.d:
	$(INTRO_PARSE) --cfg $(ICFG) $(CPPFLAGS) -MM -MG -MT 'db_$*' $*.c > $@

$(INTRO_SHARED): ../lib/introlib.c ../lib/intro.h
	$(CC) -o $@ $< -g -shared -fpic

DEPS := $(SRC:.c=.d)
ifeq (,$(filter clean,$(MAKECMDGOALS)))
include $(DEPS)
endif
