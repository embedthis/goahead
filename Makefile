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

# Probe the compiler target for CET or ARM branch protection; pass supported flags to generated builds.
CFI_FLAGS   := $(shell for f in -fcf-protection=full -mbranch-protection=standard ; do \
                   $(CC) -Werror $$f -x c -c -o /dev/null /dev/null 2>/dev/null && echo $$f && break ; done)
# Probe the compiler for the extra sanitizer groups it supports (clang: integer, implicit-conversion; gcc: bounds-strict).
# clang links the shared runtime with -shared-libsan, as the preloaded runtime of test-asan requires.
SANITIZE_FLAGS := $(shell for f in -fsanitize=integer -fsanitize=implicit-conversion -fsanitize=bounds-strict \
                   -fsanitize=float-cast-overflow -shared-libsan ; do \
                   $(CC) -Werror $$f -x c -c -o /dev/null /dev/null 2>/dev/null && echo $$f ; done)
# A shared sanitizer runtime outside the loader path (clang) is found through an rpath on every sanitized link.
SANITIZE_LDFLAGS := $(shell f=$$($(CC) -print-file-name=libclang_rt.asan-$$($(CC) -dumpmachine | cut -d- -f1).so 2>/dev/null) ; \
                   [ -f "$$f" ] && echo "-Wl,-rpath,$$(dirname $$f)")
PATH        := $(BIN):$(PATH)
CDPATH      :=

.EXPORT_ALL_VARIABLES:

.PHONY: all build clean help test test-asan test-linux

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
	tm $(if $(ARGS),$(ARGS),test) ; rc=$$? ; bash test/utils/check-server.sh server-log.txt server-exit.txt server-orphans.txt || rc=1 ; exit $$rc

# Run Linux sanitizer builds with preloaded runtimes for unsanitized test programs. CC=clang selects the clang
# runtimes and the clang-only integer and implicit-conversion checks. A canary proves the build reports before tm runs.
# Allow allocation failure and disable leak detection. Rebuild each run and remove products afterwards.
# Like test, fail on a server crash or sanitizer report that no test observed, such as one at shutdown.
# From macOS use make test-linux ARGS=--asan.
ifneq ($(findstring clang,$(shell $(CC) --version 2>&1)),)
    SAN_ARCH     = $(shell $(CC) -dumpmachine | cut -d- -f1)
    SAN_RUNTIMES = $(shell $(CC) -print-file-name=libclang_rt.asan-$(SAN_ARCH).so):$(shell $(CC) -print-file-name=libclang_rt.ubsan_standalone-$(SAN_ARCH).so)
else
    SAN_RUNTIMES = $(shell $(CC) -print-file-name=libasan.so):$(shell $(CC) -print-file-name=libubsan.so)
endif
test-asan:
ifneq ($(PLATFORM),linux)
	$(error test-asan runs on Linux. From macOS use: make test-linux ARGS=--asan)
endif
	rm -fr $(BUILD)/bin $(BUILD)/obj/*-asan
	$(MAKE) OPTIMIZE=asan build
	LD_PRELOAD="$(SAN_RUNTIMES)" \
		ASAN_OPTIONS=detect_leaks=0:allocator_may_return_null=1:detect_stack_use_after_return=1:strict_string_checks=1 \
		UBSAN_OPTIONS=print_stacktrace=1 \
		bash -c 'test/utils/sanitizer-canary.sh && tm $(if $(ARGS),$(ARGS),test)' ; rc=$$? ; \
		bash test/utils/check-server.sh server-log.txt server-exit.txt server-orphans.txt || rc=1 ; rm -fr $(BUILD)/bin ; exit $$rc

#   Run the suite under Linux on macOS. ARGS passes through to tm, e.g. ARGS=auth.
test-linux:
	@bash bin/test-linux.sh $(ARGS)

help:
	@echo '' >&2
	@echo 'usage: make [clean, build, test]' >&2
	@echo '' >&2
	@echo 'Targets:' >&2
	@echo '  build               Build libgoahead and executables (default)' >&2
	@echo '  clean               Remove build artifacts' >&2
	@echo '  test                Run unit tests' >&2
	@echo '  test-asan           Run unit tests against a sanitized build (Linux)' >&2
	@echo '  test-linux          Run unit tests under Linux in a container (macOS host)' >&2
	@echo '' >&2
	@echo 'Make variables:' >&2
	@echo '  OPTIMIZE=debug|release|asan   Build configuration (default: debug)' >&2
	@echo '  SHOW=1                        Show build commands' >&2
	@echo '  ARGS="..."                    Arguments passed to tm by test, test-asan and test-linux' >&2
	@echo '' >&2

ifneq ($(LOCAL),)
include $(LOCAL)
endif
