INTROLIB := ../lib/introlib.o
INTRO_PARSE := ../intro
CPPFLAGS += -I../lib
CFLAGS := -g
CFLAGS += $(CPPFLAGS)
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

run: $(TESTS)
	$(foreach x,$(TESTS),./$(x)$(\n))
	@echo "All tests were successful."

%.intro: % $(INTRO_PARSE)
	$(INTRO_PARSE) $(CPPFLAGS) $< -o $@

$(EXE): db_%: %.o $(INTROLIB)
	$(CC) $^ -o $@

clean:
	rm -f *.intro *.o *.d $(EXE)

deps.d: $(MAKEFILE_LIST) $(INTRO_PARSE)
	> deps.d
	$(foreach f,$(SRC),$(INTRO_PARSE) $(CPPFLAGS) -MM -MG -MT '$(f:.c=.o) deps.d' $(f) >> deps.d$(\n))

ifneq ($(MAKECMDGOALS),clean)
include deps.d
endif
