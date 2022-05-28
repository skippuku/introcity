INTROLIB := ../build/debug/introlib.o
INTRO_PARSE := ../intro
CPPFLAGS += -I../lib
CFLAGS = -g -MMD -MT '$@'
SRC = $(wildcard *.c)
INTERACTIVE := interactive_test
EXE = $(SRC:%.c=db_%)
TESTS = $(filter-out db_$(INTERACTIVE),$(EXE))

export ASAN_OPTIONS = detect_leaks=0

.PHONY: clean run
.PRECIOUS: %.intro

all: $(EXE)
	@echo "Build complete."

run: $(TESTS)
	@./../scripts/run_tests.sh $(TESTS)

$(INTRO_PARSE):
	@$(MAKE) -C .. debug

%.intro: % $(INTRO_PARSE) FORCE
	$(INTRO_PARSE) $(CPPFLAGS) $< -o $@

FORCE:

$(EXE): db_%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(INTROLIB) -o $@

clean:
	rm -f *.intro *.o *.d $(EXE)

%.d:
	$(INTRO_PARSE) $(CPPFLAGS) -MM -MG -MT 'db_$*' $*.c > $@

DEPS := $(SRC:.c=.d)
ifeq (,$(filter clean,$(MAKECMDGOALS)))
include $(DEPS)
endif
