# Builds every *.c in this directory into its own binary.
#
# Each source file is a bare snippet (statements only). For every one of them a
# glue file is generated in $(BUILD_DIR)/ that provides main(), the parallel
# region and the single construct, and #include's the snippet in the middle.
#
#   make                 build everything into build/
#   make run             build and run every binary
#   make run-1-write-read   build and run a single one
#   make clean
#
# Override the toolchain as needed, e.g.:
#   make CC=clang OMPFLAGS="-fopenmp -fopenmp-targets=nvptx64"

ifeq ($(origin CC),default)
CC := xkcxx
endif
CFLAGS    ?= -O0 -g -Wall
OMPFLAGS  ?= -fopenmp
LDFLAGS   ?=
LDLIBS    ?=
BUILD_DIR ?= build

SRCS  := $(sort $(wildcard *.cc))
NAMES := $(SRCS:.cc=)
GLUES := $(addprefix $(BUILD_DIR)/,$(addsuffix .glue.cc,$(NAMES)))
BINS  := $(addprefix $(BUILD_DIR)/,$(NAMES))

.PHONY: all run clean $(addprefix run-,$(NAMES))

all: $(BINS)

# ---- glue generation -------------------------------------------------------
$(GLUES): $(BUILD_DIR)/%.glue.cc: %.cc | $(BUILD_DIR)
	@echo "  GEN     $@"
	@printf '%s\n' \
	    '#include <assert.h>' \
	    '#include <omp.h>' \
	    '#include <stdio.h>' \
	    '#include <stdlib.h>' \
	    '' \
	    'int main(void)' \
	    '{' \
	    '    #pragma omp parallel' \
	    '    {' \
	    '        #pragma omp single' \
	    '        {' \
	    '#include "$(abspath $<)"' \
	    '        }' \
	    '    }' \
	    '    return 0;' \
	    '}' > $@

# ---- compilation (also rebuilt when the original snippet changes) ----------
$(BINS): $(BUILD_DIR)/%: $(BUILD_DIR)/%.glue.cc %.cc
	@echo "  CC      $@"
	@-$(CC) $(CFLAGS) $(OMPFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR):
	@mkdir -p $@

# ---- running ---------------------------------------------------------------
run: all
	@for b in $(BINS); do \
	    echo "=== $$b"; \
	    "$$b" || echo "!!! $$b exited with status $$?"; \
	done

$(addprefix run-,$(NAMES)): run-%: $(BUILD_DIR)/%
	@"$(BUILD_DIR)/$*"

clean:
	rm -rf $(BUILD_DIR)
