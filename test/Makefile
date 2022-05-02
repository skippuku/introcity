INTROLIB := ../lib/introlib.o
INTRO_PARSE := ../intro
CFLAGS := -g
CFLAGS += -I../lib
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
	$(CPP) $< $(CFLAGS) -D__INTRO__ | $(INTRO_PARSE) - -o $@

$(EXE): db_%: %.o $(INTROLIB)
	$(CC) $^ -o $@

clean:
	rm -f *.intro *.o *.d $(EXE)

deps.d:
	> deps.d
	$(foreach f,$(SRC),$(CC) $(CFLAGS) -MM -MG -MT '$(f:.c=.o) deps.d' $(f) >> deps.d$(\n))

ifneq ($(MAKECMDGOALS),clean)
include deps.d
endif
