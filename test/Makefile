INTROLIB := ../lib/introlib.o
INTRO_PARSE := ../intro
CPPFLAGS += -I../lib
CFLAGS := -g -MMD
SRC = $(wildcard *.c)
INTERACTIVE := interactive_test
EXE = $(SRC:%.c=db_%)
TESTS = $(filter-out db_$(INTERACTIVE),$(EXE))

define \n


endef

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

$(EXE): db_%: %.o $(INTROLIB)
	$(CC) $^ -o $@

clean:
	rm -f *.intro *.o *.d $(EXE)

%.d:
	$(INTRO_PARSE) $(CPPFLAGS) -MM -MG $*.c > $@

DEPS := $(SRC:.c=.d)
ifeq (,$(filter clean,$(MAKECMDGOALS)))
include $(DEPS)
endif
