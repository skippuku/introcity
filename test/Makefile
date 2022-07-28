INTROLIB := ../build/debug/introlib.o
INTRO_PARSE := ../build/debug/intro
CPPFLAGS += -I../lib
CFLAGS = -g -MMD -MT '$@'
SRC := $(wildcard *.c)
INTERACTIVE := interactive_test
EXE := $(SRC:%.c=db_%)
TESTS := $(filter-out db_$(INTERACTIVE),$(EXE))
ICFG := ../intro.cfg

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

$(INTROLIB): FORCE
	@$(MAKE) -C .. debug

$(ICFG):
	@$(MAKE) -C .. config

%.intro: % $(INTRO_PARSE) FORCE
	$(INTRO_PARSE) --cfg $(ICFG) $(CPPFLAGS) $< -o $@

intro.h.intro: ../lib/intro.h $(INTRO_PARSE) FORCE
	$(INTRO_PARSE) --pragma "enable all" --cfg $(ICFG) $(CPPFLAGS) $< -o $@

FORCE:

$(EXE): db_%: %.c $(INTROLIB)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(INTROLIB) -o $@

clean:
	rm -f *.intro *.o *.d $(EXE)

%.d:
	$(INTRO_PARSE) --cfg $(ICFG) $(CPPFLAGS) -MM -MG -MT 'db_$*' $*.c > $@

DEPS := $(SRC:.c=.d)
ifeq (,$(filter clean,$(MAKECMDGOALS)))
include $(DEPS)
endif
