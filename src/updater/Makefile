#
#	Update Makefile
#
OS := $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')

ifeq ($(OS),macosx)
	IFLAGS	:= -I /opt/homebrew/include
	LFLAGS	:= -L /opt/homebrew/lib
endif

all: compile

compile build: updater

updater.o: updater.c
	clang -g $(IFLAGS) -c updater.c

updater: updater.o 
	clang $(IFLAGS) $(LFLAGS) -o updater main.c updater.o -lssl -lcrypto

clean:
	rm -f updater.o main.o updater updater.bin

cache: clean
	cp README* apply.sh main.c updater.h updater.c Makefile dist
	cache

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
