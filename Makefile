#
#   Makefile - Embedthis GoAhead Makefile wrapper for per-platform makefiles
#
#	This Makefile is for Unix/Linux and Cygwin. On windows, it can be invoked via make.bat.
#
#   You can use this Makefile and build via "make" with a pre-selected configuration. Alternatively,
#   you can build using the MakeMe tool for for a fully configurable build. If you wish to
#   cross-compile, you should use MakeMe.
#
#	See projects/$(OS)-$(ARCH)-$(PROFILE)-me.h for configuration default settings. Can override
#	via make environment variables. For example: make ME_COM_SQLITE=0. These are converted to
#	DFLAGS and will then override the me.h default values. Use "make help" for a list of available
#	make variable options.
#
NAME    := goahead
OS      := $(shell uname | sed 's/CYGWIN.*/windows/;s/Darwin/macosx/' | tr '[A-Z]' '[a-z]')
PROFILE := default

ifeq ($(ARCH),)
	ifeq ($(OS),windows)
		ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
			ARCH?=x64
		else
			ARCH?=x86
		endif
	else
		ARCH:= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/mips.*/mips/')
	endif
endif

ifeq ($(OS),windows)
    MAKE	:= MAKEFLAGS= projects/windows.bat $(ARCH) nmake -nologo
    EXT 	:= nmake
else
	MAKE    := $(shell if which gmake >/dev/null 2>&1; then echo gmake ; else echo make ; fi) --no-print-directory
	EXT     := mk
endif

BIN 	:= $(OS)-$(ARCH)-$(PROFILE)/bin
PATH    := $(PWD)/build/$(BIN):$(PATH)

.EXPORT_ALL_VARIABLES:

all compile:
	@if [ ! -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) ] ; then \
		echo "The build configuration projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) is not supported" ; exit 255 ; \
	fi
	@echo $(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@
	@$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@
	@echo ; echo 'On Linux/MacOS, you can now install via "sudo make $(MAKEFLAGS) install" or run GoAhead via: "sudo make run"'
	@echo "To run locally, put $(OS)-$(ARCH)-$(PROFILE)/bin in your path" ; echo

clean clobber installBinary uninstall run:
	@$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@

deploy:
	@echo '       [Deploy] $(MAKE) ME_ROOT_PREFIX=$(OS)-$(ARCH)-$(PROFILE)/deploy -f projects/$(NAME)-$(OS)-$(PROFILE).  $(EXT) installBinary'
	@$(MAKE) ME_ROOT_PREFIX=$(OS)-$(ARCH)-$(PROFILE)/deploy -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) installBinary

install:
	$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@
ifneq ($(OS),windows)
	@echo ; echo 'You can now run via "sudo goahead -v --home /etc/goahead /var/www/goahead"'
else
	@echo ; echo 'You can now run via "goahead -v" in the goahead installation directory.'
endif

version:
	@$(MAKE) -f projects/$(NAME)-$(OS)-$(PROFILE).$(EXT) $@

help:
	@echo '' >&2
	@echo 'usage: make [clean, compile, deploy, install, run, uninstall]' >&2
	@echo '' >&2
	@echo 'With make, the default configuration can be modified by setting make' >&2
	@echo 'variables. Set to 0 to disable and 1 to enable:' >&2
	@echo '' >&2
	@echo '  ME_GOAHEAD_ACCESS_LOG             # Enable request access log (true|false)' >&2
	@echo '  ME_GOAHEAD_CLIENT_CACHE           # List of extensions to cache in the client' >&2
	@echo '  ME_GOAHEAD_CLIENT_CACHE_LIFESPAN  # Time in seconds to cache in the client' >&2
	@echo '  ME_GOAHEAD_CA_FILE                # File of client certificates (path)' >&2
	@echo '  ME_GOAHEAD_CERTIFICATE            # Server certificate for SSL (path)' >&2
	@echo '  ME_GOAHEAD_SSL_CIPHERS            # SSL cipher suite (string)' >&2
	@echo '  ME_GOAHEAD_CGI                    # Enable the CGI handler (true|false)' >&2
	@echo '  ME_GOAHEAD_CGI_BIN                # Directory CGI programs (path)' >&2
	@echo '  ME_GOAHEAD_JAVASCRIPT             # Enable the Javascript JST handler (true|false)' >&2
	@echo '  ME_GOAHEAD_KEY                    # Server private key for SSL (path)' >&2
	@echo '  ME_GOAHEAD_LEGACY                 # Enable the GoAhead 2.X legacy APIs (true|false)' >&2
	@echo '  ME_GOAHEAD_LIMIT_BUFFER           # I/O Buffer size. Also chunk size.' >&2
	@echo '  ME_GOAHEAD_LIMIT_FILENAME         # Maximum filename size' >&2
	@echo '  ME_GOAHEAD_LIMIT_HEADER           # Maximum HTTP single header size' >&2
	@echo '  ME_GOAHEAD_LIMIT_HEADERS          # Maximum HTTP header size' >&2
	@echo '  ME_GOAHEAD_LIMIT_NUM_HEADERS      # Maximum number of headers' >&2
	@echo '  ME_GOAHEAD_LIMIT_PASSWORD         # Maximum password size' >&2
	@echo '  ME_GOAHEAD_LIMIT_POST             # Maximum incoming body size' >&2
	@echo '  ME_GOAHEAD_LIMIT_PUT              # Maximum PUT body size ~ 200MB' >&2
	@echo '  ME_GOAHEAD_LIMIT_SESSION_LIFE     # Session lifespan in seconds (30 mins)' >&2
	@echo '  ME_GOAHEAD_LIMIT_SESSION_COUNT    # Maximum number of sessions to support' >&2
	@echo '  ME_GOAHEAD_LIMIT_STRING           # Default string allocation size' >&2
	@echo '  ME_GOAHEAD_LIMIT_TIMEOUT          # Request inactivity timeout in seconds' >&2
	@echo '  ME_GOAHEAD_LIMIT_URI              # Maximum URI size' >&2
	@echo '  ME_GOAHEAD_LIMIT_UPLOAD           # Maximum upload size ~ 200MB' >&2
	@echo '  ME_GOAHEAD_LISTEN                 # Addresses to listen to (["http://IP:port", ...])' >&2
	@echo '  ME_GOAHEAD_LOGFILE                # Default location and level for debug log (path:level)' >&2
	@echo '  ME_GOAHEAD_LOGGING                # Enable application logging (true|false)' >&2
	@echo '  ME_GOAHEAD_PAM                    # Enable Unix Pluggable Auth Module (true|false)' >&2
	@echo '  ME_GOAHEAD_PUT_DIR                # Define the directory for file uploaded via HTTP PUT (path)' >&2
	@echo '  ME_GOAHEAD_REALM                  # Authentication realm (string)' >&2
	@echo '  ME_GOAHEAD_REPLACE_MALLOC         # Replace malloc with non-fragmenting allocator (true|false)' >&2
	@echo '  ME_GOAHEAD_SSL                    # To enable SSL' >&2
	@echo '  ME_GOAHEAD_STATIC                 # Build with static linking (true|false)' >&2
	@echo '  ME_GOAHEAD_STEALTH                # Run in stealth mode. Disable OPTIONS, TRACE (true|false)' >&2
	@echo '  ME_GOAHEAD_TRACING                # Enable debug tracing (true|false)' >&2
	@echo '  ME_GOAHEAD_TUNE                   # Optimize (size|speed|balanced)' >&2
	@echo '  ME_GOAHEAD_UPLOAD                 # Enable file upload (true|false)' >&2
	@echo '  ME_GOAHEAD_UPLOAD_DIR             # Define directory for uploaded files (path)' >&2
	@echo '  ME_COM_MBEDTLS                    # Enable the mbed TLS stack' >&2
	@echo '  ME_COM_OPENSSL                    # Enable the OpenSSL SSL stack, must set ME_COM_OPENSS_PATH' >&2
	@echo '  ME_ROM                            # Build for ROM without a file system' >&2
	@echo '  ME_STACK_SIZE                     # Define the VxWorks stack size' >&2
	@echo '' >&2
	@echo 'For example, to disable CGI:' >&2
	@echo '' >&2
	@echo '  ME_GOAHEAD_CGI=0 make' >&2
	@echo '' >&2
	@echo 'Other make environment variables:' >&2
	@echo '  ARCH               # CPU architecture (x86, x64, ppc, ...)' >&2
	@echo '  OS                 # Operating system (linux, macosx, windows, vxworks, ...)' >&2
	@echo '  CC                 # Compiler to use ' >&2
	@echo '  LD                 # Linker to use' >&2
	@echo '  CONFIG             # Output directory for built items. Defaults to OS-ARCH-PROFILE' >&2
	@echo '  CFLAGS             # Add compiler options. For example: -Wall' >&2
	@echo '  DEBUG              # Set to "debug" for symbols, "release" for optimized builds' >&2
	@echo '  DFLAGS             # Add compiler defines. For example: -DCOLOR=blue' >&2
	@echo '  IFLAGS             # Add compiler include directories. For example: -I/extra/includes' >&2
	@echo '  LDFLAGS            # Add linker options' >&2
	@echo '  LIBPATHS           # Add linker library search directories. For example: -L/libraries' >&2
	@echo '  LIBS               # Add linker libraries. For example: -lpthreads' >&2
	@echo '  PROFILE            # Set to "static" for static linking or "default" for dynamic' >&2
	@echo '' >&2
	@echo 'Use "SHOW=1 make" to show executed commands.' >&2
	@echo '' >&2

LOCAL_MAKEFILE := $(strip $(wildcard ./.local.mk))

ifneq ($(LOCAL_MAKEFILE),)
include $(LOCAL_MAKEFILE)
endif
