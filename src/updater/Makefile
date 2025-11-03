#
#	Updater src Makefile
#
OS 	:= $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')

ifeq ($(OS),macosx)
	IFLAGS	:= -I . -I osdep -I /opt/homebrew/include
	LFLAGS	:= -L /opt/homebrew/lib
endif

all: compile

compile build: updater

updater: updater.c
	clang $(IFLAGS) $(LFLAGS) -o updater main.c updater.c -lssl -lcrypto

clean:
	@rm -f updater.o main.o updater
