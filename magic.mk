# from github.com/cyman-ide/magic.mk
PROFILE := $(filter-out all,$(firstword $(MAKECMDGOALS)))
ifeq (,$(PROFILE))
    ifndef MAGIC_DEFAULT_PROFILE
        $(error No profile and MAGIC_DEFAULT_PROFILE is not defined.)
    endif
    PROFILE := $(MAGIC_DEFAULT_PROFILE)
    MAKECMDGOALS := $(PROFILE)
endif

ifeq ($(flavor PROFILE.$(PROFILE)), undefined)
    $(error Invalid profile: $(PROFILE))
endif

$(eval $(PROFILE.$(PROFILE)))

.PHONY: $(MAKECMDGOALS)
$(MAKECMDGOALS): $(MAGIC_TARGET)

BUILDDIR ?= build
PROFILEDIR ?= $(PROFILE)
OBJDIR := $(BUILDDIR)/$(PROFILEDIR)
DEPGENFLAGS ?= -MMD

getobj = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(notdir $(1)))))
OBJ := $(call getobj,$(SRC))
DEPS := $(OBJ:%.o=%.d)

$(foreach f,$(SRC),$(eval $(call getobj,$f): $f))

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(DEPS): %.d: %.o

$(OBJ): | $(OBJDIR)
	$(COMPILE$(suffix $<)) $(DEPGENFLAGS) $< $(OUTPUT_OPTION)

ifndef MAGIC_NODEP
    include $(DEPS)
endif
