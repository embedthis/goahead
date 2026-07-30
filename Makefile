#
#   Makefile -- GoAhead Top-level Makefile
#
#   Uses pre-generated project files from projects/gmake2/.
#   Auto-detects platform. No premake5 required for building.
#
#   Use "make help" for available targets and options.
#

NAME        := goahead
OPTIMIZE    ?= debug
TOP         := $(shell realpath .)
BUILD       := build
BIN         := $(TOP)/$(BUILD)/bin
LOCAL       := $(strip $(wildcard ./.local.mk))

#
#   Detect make command (prefer gmake)
#
MAKE        := $(shell if which gmake >/dev/null 2>&1; then echo gmake ; else echo make ; fi) --no-print-directory

#
#   Auto-detect platform from host OS
#
UNAME       := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    PLATFORM := macosx
else ifeq ($(UNAME),Linux)
    PLATFORM := linux
else ifeq ($(UNAME),FreeBSD)
    PLATFORM := freebsd
else
    $(error Unsupported platform: $(UNAME). Use premake5 to regenerate for your OS.)
endif

CONFIG      := $(OPTIMIZE)_$(PLATFORM)
PATH        := $(BIN):$(PATH)
CDPATH      :=

.EXPORT_ALL_VARIABLES:

.PHONY: all build clean help package test

ifndef SHOW
.SILENT:
endif

all: build

build:
	@if [ ! -f projects/gmake2/Makefile ] ; then \
		echo "      [Error] projects/gmake2/Makefile not found. Run: cd projects && premake5 gmake" ; exit 255 ; \
	fi
	$(MAKE) -C projects/gmake2 config=$(CONFIG) verbose=$(SHOW)
	@echo "      [Info] GoAhead $(OPTIMIZE) [$(PLATFORM)]"

clean:
	@echo "       [Run] clean"
	rm -fr $(BUILD)

test: build
	tm test

package:
	bash bin/buildLib.sh

help:
	@echo '' >&2
	@echo 'usage: make [clean, build, test]' >&2
	@echo '' >&2
	@echo 'Targets:' >&2
	@echo '  build               Build libgoahead and executables (default)' >&2
	@echo '  clean               Remove build artifacts' >&2
	@echo '  test                Run unit tests' >&2
	@echo '  package             Build dist/goaheadLib.c amalgamated source' >&2
	@echo '' >&2
	@echo 'Make variables:' >&2
	@echo '  OPTIMIZE=debug|release    Optimization level (default: debug)' >&2
	@echo '  SHOW=1                    Show build commands' >&2
	@echo '' >&2

ifneq ($(LOCAL),)
include $(LOCAL)
endif
