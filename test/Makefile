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

CRED   := \e[31m
CGREEN := \e[32m
CCYAN  := \e[36m
CRESET := \e[0m

.ONESHELL:
run: $(TESTS)
	@echo > log.txt
	fails=0
	for f in $(TESTS); do
		printf "\n----------------\nTEST $$f\n----------------\n" >> log.txt
		printf "%-30b" "TEST $(CCYAN)$$f$(CRESET)"
		err=$$(./$$f 2>&1 >> log.txt)
		ok=$$?
		printf -- "$$err" >> log.txt
		if [ $$ok -eq 0 ]; then
			printf "$(CGREEN)OK"
		else
			printf "$(CRED)FAILED$(CRESET)\n"
			printf "    $$err"
			fails=1
		fi
		printf "$(CRESET)\n"
	done
	[ $$fails -eq 0 ] \
	  && printf "$(CGREEN)All tests passed." \
	  || printf "$(CRED)Some tests failed."
	printf "$(CRESET)\n"

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
