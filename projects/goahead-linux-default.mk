#
#   goahead-linux-default.mk -- Makefile to build Embedthis GoAhead for linux
#

NAME                  := goahead
VERSION               := 6.0.5
PROFILE               ?= default
ARCH                  ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/mips.*/mips/')
CC_ARCH               ?= $(shell echo $(ARCH) | sed 's/x86/i686/;s/x64/x86_64/')
OS                    ?= linux
CC                    ?= gcc
AR                    ?= ar
BUILD                 ?= build/$(OS)-$(ARCH)-$(PROFILE)
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

#
# Components
#
ME_COM_COMPILER       ?= 1
ME_COM_LIB            ?= 1
ME_COM_MBEDTLS        ?= 0
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 0


ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_MBEDTLS),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_OPENSSL),1)
    ME_COM_SSL := 1
endif

#
# Settings
#
ME_AUTHOR             ?= \"Embedthis Software\"
ME_CERTS_GENDH        ?= 1
ME_COMPANY            ?= \"embedthis\"
ME_COMPATIBLE         ?= \"6.0\"
ME_COMPILER_FORTIFY   ?= 1
ME_COMPILER_HAS_ATOMIC ?= 1
ME_COMPILER_HAS_ATOMIC64 ?= 1
ME_COMPILER_HAS_DOUBLE_BRACES ?= 1
ME_COMPILER_HAS_DYN_LOAD ?= 1
ME_COMPILER_HAS_LIB_EDIT ?= 0
ME_COMPILER_HAS_LIB_RT ?= 1
ME_COMPILER_HAS_MMU   ?= 1
ME_COMPILER_HAS_MTUNE ?= 1
ME_COMPILER_HAS_PAM   ?= 0
ME_COMPILER_HAS_STACK_PROTECTOR ?= 1
ME_COMPILER_HAS_SYNC  ?= 1
ME_COMPILER_HAS_SYNC64 ?= 1
ME_COMPILER_HAS_SYNC_CAS ?= 1
ME_COMPILER_HAS_UNNAMED_UNIONS ?= 1
ME_COMPILER_NOEXECSTACK ?= 1
ME_COMPILER_WARN64TO32 ?= 0
ME_COMPILER_WARN_UNUSED ?= 1
ME_CONFIGURE          ?= \"me -d -q -platform linux-x64-default -configure . -gen make\"
ME_CONFIGURED         ?= 1
ME_DEBUG              ?= 1
ME_DEPRECATED_WARNINGS ?= 0
ME_DEPTH              ?= 1
ME_DESCRIPTION        ?= \"Embedthis GoAhead\"
ME_GOAHEAD_ACCESS_LOG ?= 0
ME_GOAHEAD_AUTH       ?= 1
ME_GOAHEAD_AUTH_STORE ?= \"file\"
ME_GOAHEAD_AUTO_LOGIN ?= 0
ME_GOAHEAD_CGI        ?= 1
ME_GOAHEAD_CGI_VAR_PREFIX ?= \"CGI_\"
ME_GOAHEAD_CLIENT_CACHE ?= \"css,gif,ico,jpg,js,png\"
ME_GOAHEAD_CLIENT_CACHE_LIFESPAN ?= 86400
ME_GOAHEAD_DIGEST     ?= 1
ME_GOAHEAD_DOCUMENTS  ?= \"web\"
ME_GOAHEAD_JAVASCRIPT ?= 1
ME_GOAHEAD_LEGACY     ?= 0
ME_GOAHEAD_LIMIT_BUFFER ?= 1024
ME_GOAHEAD_LIMIT_CGI_ARGS ?= 4096
ME_GOAHEAD_LIMIT_FILENAME ?= 256
ME_GOAHEAD_LIMIT_FILES ?= 0
ME_GOAHEAD_LIMIT_HEADER ?= 2048
ME_GOAHEAD_LIMIT_HEADERS ?= 4096
ME_GOAHEAD_LIMIT_NUM_HEADERS ?= 64
ME_GOAHEAD_LIMIT_PARSE_TIMEOUT ?= 5
ME_GOAHEAD_LIMIT_PASSWORD ?= 32
ME_GOAHEAD_LIMIT_POST ?= 16384
ME_GOAHEAD_LIMIT_PUT  ?= 204800000
ME_GOAHEAD_LIMIT_SESSION_COUNT ?= 512
ME_GOAHEAD_LIMIT_SESSION_LIFE ?= 1800
ME_GOAHEAD_LIMIT_STRING ?= 256
ME_GOAHEAD_LIMIT_TIMEOUT ?= 60
ME_GOAHEAD_LIMIT_UPLOAD ?= 204800000
ME_GOAHEAD_LIMIT_URI  ?= 2048
ME_GOAHEAD_LISTEN     ?= \"http://*:80,https://*:443\"
ME_GOAHEAD_LOGFILE    ?= \"stderr:0\"
ME_GOAHEAD_LOGGING    ?= 1
ME_GOAHEAD_PUT_DIR    ?= \".\"
ME_GOAHEAD_REALM      ?= \"example.com\"
ME_GOAHEAD_REPLACE_MALLOC ?= 0
ME_GOAHEAD_SSL_AUTHORITY ?= \"\"
ME_GOAHEAD_SSL_CACHE  ?= 512
ME_GOAHEAD_SSL_CERTIFICATE ?= \"self.crt\"
ME_GOAHEAD_SSL_CIPHERS ?= \"\"
ME_GOAHEAD_SSL_HANDSHAKES ?= 3
ME_GOAHEAD_SSL_KEY    ?= \"self.key\"
ME_GOAHEAD_SSL_LOG_LEVEL ?= 5
ME_GOAHEAD_SSL_REVOKE ?= \"\"
ME_GOAHEAD_SSL_TICKET ?= 1
ME_GOAHEAD_SSL_TIMEOUT ?= 86400
ME_GOAHEAD_SSL_VERIFY_ISSUER ?= 0
ME_GOAHEAD_SSL_VERIFY_PEER ?= 0
ME_GOAHEAD_STEALTH    ?= 1
ME_GOAHEAD_TRACING    ?= 1
ME_GOAHEAD_UPLOAD     ?= 1
ME_GOAHEAD_UPLOAD_DIR ?= \"tmp\"
ME_GOAHEAD_XFRAME_HEADER ?= \"SAMEORIGIN\"
ME_INTEGRATE          ?= 1
ME_MANIFEST           ?= \"installs/manifest.me\"
ME_MBEDTLS_COMPACT    ?= 1
ME_NAME               ?= \"goahead\"
ME_OPENSSL_VERSION    ?= \"1.0\"
ME_PARTS              ?= \"undefined\"
ME_PREFIXES           ?= \"install-prefixes\"
ME_ROM                ?= 0
ME_ROM_TIME           ?= 1505449519432
ME_TITLE              ?= \"Embedthis GoAhead\"
ME_VERSION            ?= \"6.0.5\"

CFLAGS                += -fPIC -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Wl,-z,relro,-z,now -Wl,--as-needed -Wl,--no-copy-dt-needed-entries -Wl,-z,noexecheap -Wl,-z,noexecstack -Wl,--no-warn-execstack  -w
DFLAGS                += -D_REENTRANT -DPIC $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_MBEDTLS=$(ME_COM_MBEDTLS) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_CERTS_GENDH=$(ME_CERTS_GENDH) -DME_GOAHEAD_ACCESSLOG=$(ME_GOAHEAD_ACCESSLOG) -DME_GOAHEAD_AUTH=$(ME_GOAHEAD_AUTH) -DME_GOAHEAD_AUTHSTORE=$(ME_GOAHEAD_AUTHSTORE) -DME_GOAHEAD_AUTOLOGIN=$(ME_GOAHEAD_AUTOLOGIN) -DME_GOAHEAD_CGI=$(ME_GOAHEAD_CGI) -DME_GOAHEAD_CGIVARPREFIX=$(ME_GOAHEAD_CGIVARPREFIX) -DME_GOAHEAD_CLIENTCACHE=$(ME_GOAHEAD_CLIENTCACHE) -DME_GOAHEAD_CLIENTCACHELIFESPAN=$(ME_GOAHEAD_CLIENTCACHELIFESPAN) -DME_GOAHEAD_DIGEST=$(ME_GOAHEAD_DIGEST) -DME_GOAHEAD_DOCUMENTS=$(ME_GOAHEAD_DOCUMENTS) -DME_GOAHEAD_JAVASCRIPT=$(ME_GOAHEAD_JAVASCRIPT) -DME_GOAHEAD_LEGACY=$(ME_GOAHEAD_LEGACY) -DME_GOAHEAD_LIMITBUFFER=$(ME_GOAHEAD_LIMITBUFFER) -DME_GOAHEAD_LIMITCGIARGS=$(ME_GOAHEAD_LIMITCGIARGS) -DME_GOAHEAD_LIMITFILENAME=$(ME_GOAHEAD_LIMITFILENAME) -DME_GOAHEAD_LIMITFILES=$(ME_GOAHEAD_LIMITFILES) -DME_GOAHEAD_LIMITHEADER=$(ME_GOAHEAD_LIMITHEADER) -DME_GOAHEAD_LIMITHEADERS=$(ME_GOAHEAD_LIMITHEADERS) -DME_GOAHEAD_LIMITNUMHEADERS=$(ME_GOAHEAD_LIMITNUMHEADERS) -DME_GOAHEAD_LIMITPARSETIMEOUT=$(ME_GOAHEAD_LIMITPARSETIMEOUT) -DME_GOAHEAD_LIMITPASSWORD=$(ME_GOAHEAD_LIMITPASSWORD) -DME_GOAHEAD_LIMITPOST=$(ME_GOAHEAD_LIMITPOST) -DME_GOAHEAD_LIMITPUT=$(ME_GOAHEAD_LIMITPUT) -DME_GOAHEAD_LIMITSESSIONCOUNT=$(ME_GOAHEAD_LIMITSESSIONCOUNT) -DME_GOAHEAD_LIMITSESSIONLIFE=$(ME_GOAHEAD_LIMITSESSIONLIFE) -DME_GOAHEAD_LIMITSTRING=$(ME_GOAHEAD_LIMITSTRING) -DME_GOAHEAD_LIMITTIMEOUT=$(ME_GOAHEAD_LIMITTIMEOUT) -DME_GOAHEAD_LIMITUPLOAD=$(ME_GOAHEAD_LIMITUPLOAD) -DME_GOAHEAD_LIMITURI=$(ME_GOAHEAD_LIMITURI) -DME_GOAHEAD_LISTEN=$(ME_GOAHEAD_LISTEN) -DME_GOAHEAD_LOGFILE=$(ME_GOAHEAD_LOGFILE) -DME_GOAHEAD_LOGGING=$(ME_GOAHEAD_LOGGING) -DME_GOAHEAD_PUTDIR=$(ME_GOAHEAD_PUTDIR) -DME_GOAHEAD_REALM=$(ME_GOAHEAD_REALM) -DME_GOAHEAD_REPLACE_MALLOC=$(ME_GOAHEAD_REPLACE_MALLOC) -DME_GOAHEAD_SSL=$(ME_GOAHEAD_SSL) -DME_GOAHEAD_STEALTH=$(ME_GOAHEAD_STEALTH) -DME_GOAHEAD_TRACING=$(ME_GOAHEAD_TRACING) -DME_GOAHEAD_UPLOAD=$(ME_GOAHEAD_UPLOAD) -DME_GOAHEAD_UPLOADDIR=$(ME_GOAHEAD_UPLOADDIR) -DME_GOAHEAD_XFRAMEHEADER=$(ME_GOAHEAD_XFRAMEHEADER) -DME_MBEDTLS_COMPACT=$(ME_MBEDTLS_COMPACT) -DME_OPENSSL_VERSION=$(ME_OPENSSL_VERSION) 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += -z noexecstack -g -rdynamic -Wl,--enable-new-dtags -Wl,-rpath,$$ORIGIN/
LIBPATHS              += -L$(BUILD)/bin
LIBS                  += -lrt -ldl -lpthread -lm

OPTIMIZE              ?= debug
CFLAGS-debug          ?= -g
DFLAGS-debug          ?= -DME_DEBUG=1
LDFLAGS-debug         ?= -g
DFLAGS-release        ?= 
CFLAGS-release        ?= -O2
LDFLAGS-release       ?= 
CFLAGS                += $(CFLAGS-$(OPTIMIZE))
DFLAGS                += $(DFLAGS-$(OPTIMIZE))
LDFLAGS               += $(LDFLAGS-$(OPTIMIZE))

ME_ROOT_PREFIX        ?= 
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local
ME_DATA_PREFIX        ?= $(ME_ROOT_PREFIX)/
ME_STATE_PREFIX       ?= $(ME_ROOT_PREFIX)/var
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)/lib/$(NAME)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)/$(VERSION)
ME_BIN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/bin
ME_INC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/include
ME_LIB_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/lib
ME_MAN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/share/man
ME_SBIN_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local/sbin
ME_ETC_PREFIX         ?= $(ME_ROOT_PREFIX)/etc/$(NAME)
ME_WEB_PREFIX         ?= $(ME_ROOT_PREFIX)/var/www/$(NAME)
ME_LOG_PREFIX         ?= $(ME_ROOT_PREFIX)/var/log/$(NAME)
ME_VLIB_PREFIX        ?= $(ME_ROOT_PREFIX)/var/lib/$(NAME)
ME_SPOOL_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)
ME_CACHE_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)/cache
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)$(NAME)-$(VERSION)


TARGETS               += $(BUILD)/bin/goahead
TARGETS               += $(BUILD)/bin/goahead-test
TARGETS               += $(BUILD)/bin/gopass
TARGETS               += $(BUILD)/.install-roots-modified


DEPEND := $(strip $(wildcard ./projects/depend.mk))
ifneq ($(DEPEND),)
include $(DEPEND)
endif

unexport CDPATH

ifndef SHOW
.SILENT:
endif

all build compile: prep $(TARGETS)

.PHONY: prep

prep:
	@if [ "$(BUILD)" = "" ] ; then echo WARNING: BUILD not set ; exit 255 ; fi
	@if [ "$(ME_APP_PREFIX)" = "" ] ; then echo WARNING: ME_APP_PREFIX not set ; exit 255 ; fi
	@[ ! -x $(BUILD)/bin ] && mkdir -p $(BUILD)/bin; true
	@[ ! -x $(BUILD)/inc ] && mkdir -p $(BUILD)/inc; true
	@[ ! -x $(BUILD)/obj ] && mkdir -p $(BUILD)/obj; true
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/goahead-linux-$(PROFILE)-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/goahead-linux-$(PROFILE)-me.h >/dev/null ; then\
		cp projects/goahead-linux-$(PROFILE)-me.h $(BUILD)/inc/me.h  ; \
	fi; true
	@if [ -f "$(BUILD)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != "`cat $(BUILD)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build" ; \
			echo "   [Warning] Previous build command: "`cat $(BUILD)/.makeflags`"" ; \
		fi ; \
	fi
	@echo "$(MAKEFLAGS)" >$(BUILD)/.makeflags

clean:
	rm -f "$(BUILD)/obj/action.o"
	rm -f "$(BUILD)/obj/alloc.o"
	rm -f "$(BUILD)/obj/auth.o"
	rm -f "$(BUILD)/obj/cgi.o"
	rm -f "$(BUILD)/obj/cgitest.o"
	rm -f "$(BUILD)/obj/crypt.o"
	rm -f "$(BUILD)/obj/file.o"
	rm -f "$(BUILD)/obj/fs.o"
	rm -f "$(BUILD)/obj/goahead.o"
	rm -f "$(BUILD)/obj/gopass.o"
	rm -f "$(BUILD)/obj/http.o"
	rm -f "$(BUILD)/obj/js.o"
	rm -f "$(BUILD)/obj/jst.o"
	rm -f "$(BUILD)/obj/mbedtls.o"
	rm -f "$(BUILD)/obj/openssl.o"
	rm -f "$(BUILD)/obj/options.o"
	rm -f "$(BUILD)/obj/osdep.o"
	rm -f "$(BUILD)/obj/rom.o"
	rm -f "$(BUILD)/obj/route.o"
	rm -f "$(BUILD)/obj/runtime.o"
	rm -f "$(BUILD)/obj/socket.o"
	rm -f "$(BUILD)/obj/test.o"
	rm -f "$(BUILD)/obj/time.o"
	rm -f "$(BUILD)/obj/upload.o"
	rm -f "$(BUILD)/bin/goahead"
	rm -f "$(BUILD)/bin/goahead-test"
	rm -f "$(BUILD)/bin/gopass"
	rm -f "$(BUILD)/.install-roots-modified"
	rm -f "$(BUILD)/bin/libgo.so"

clobber: clean
	rm -fr ./$(BUILD)

#
#   me.h
#

$(BUILD)/inc/me.h: $(DEPS_1)

#
#   osdep.h
#
DEPS_2 += src/osdep/osdep.h
DEPS_2 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_2)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp src/osdep/osdep.h $(BUILD)/inc/osdep.h

#
#   goahead.h
#
DEPS_3 += src/goahead.h
DEPS_3 += $(BUILD)/inc/me.h
DEPS_3 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/goahead.h: $(DEPS_3)
	@echo '      [Copy] $(BUILD)/inc/goahead.h'
	mkdir -p "$(BUILD)/inc"
	cp src/goahead.h $(BUILD)/inc/goahead.h

#
#   js.h
#
DEPS_4 += src/js.h
DEPS_4 += $(BUILD)/inc/goahead.h

$(BUILD)/inc/js.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/js.h'
	mkdir -p "$(BUILD)/inc"
	cp src/js.h $(BUILD)/inc/js.h

#
#   action.o
#
DEPS_5 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/action.o: \
    src/action.c $(DEPS_5)
	@echo '   [Compile] $(BUILD)/obj/action.o'
	$(CC) -c -o $(BUILD)/obj/action.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/action.c

#
#   alloc.o
#
DEPS_6 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/alloc.o: \
    src/alloc.c $(DEPS_6)
	@echo '   [Compile] $(BUILD)/obj/alloc.o'
	$(CC) -c -o $(BUILD)/obj/alloc.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/alloc.c

#
#   auth.o
#
DEPS_7 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/auth.o: \
    src/auth.c $(DEPS_7)
	@echo '   [Compile] $(BUILD)/obj/auth.o'
	$(CC) -c -o $(BUILD)/obj/auth.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/auth.c

#
#   cgi.o
#
DEPS_8 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/cgi.o: \
    src/cgi.c $(DEPS_8)
	@echo '   [Compile] $(BUILD)/obj/cgi.o'
	$(CC) -c -o $(BUILD)/obj/cgi.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/cgi.c

#
#   cgitest.o
#

$(BUILD)/obj/cgitest.o: \
    test/cgitest.c $(DEPS_9)
	@echo '   [Compile] $(BUILD)/obj/cgitest.o'
	$(CC) -c -o $(BUILD)/obj/cgitest.o $(CFLAGS) $(DFLAGS) $(IFLAGS) test/cgitest.c

#
#   crypt.o
#
DEPS_10 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/crypt.o: \
    src/crypt.c $(DEPS_10)
	@echo '   [Compile] $(BUILD)/obj/crypt.o'
	$(CC) -c -o $(BUILD)/obj/crypt.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/crypt.c

#
#   file.o
#
DEPS_11 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/file.o: \
    src/file.c $(DEPS_11)
	@echo '   [Compile] $(BUILD)/obj/file.o'
	$(CC) -c -o $(BUILD)/obj/file.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/file.c

#
#   fs.o
#
DEPS_12 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/fs.o: \
    src/fs.c $(DEPS_12)
	@echo '   [Compile] $(BUILD)/obj/fs.o'
	$(CC) -c -o $(BUILD)/obj/fs.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/fs.c

#
#   goahead.o
#
DEPS_13 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/goahead.o: \
    src/goahead.c $(DEPS_13)
	@echo '   [Compile] $(BUILD)/obj/goahead.o'
	$(CC) -c -o $(BUILD)/obj/goahead.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/goahead.c

#
#   gopass.o
#
DEPS_14 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/gopass.o: \
    src/utils/gopass.c $(DEPS_14)
	@echo '   [Compile] $(BUILD)/obj/gopass.o'
	$(CC) -c -o $(BUILD)/obj/gopass.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/utils/gopass.c

#
#   http.o
#
DEPS_15 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/http.o: \
    src/http.c $(DEPS_15)
	@echo '   [Compile] $(BUILD)/obj/http.o'
	$(CC) -c -o $(BUILD)/obj/http.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/http.c

#
#   js.o
#
DEPS_16 += $(BUILD)/inc/js.h

$(BUILD)/obj/js.o: \
    src/js.c $(DEPS_16)
	@echo '   [Compile] $(BUILD)/obj/js.o'
	$(CC) -c -o $(BUILD)/obj/js.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/js.c

#
#   jst.o
#
DEPS_17 += $(BUILD)/inc/goahead.h
DEPS_17 += $(BUILD)/inc/js.h

$(BUILD)/obj/jst.o: \
    src/jst.c $(DEPS_17)
	@echo '   [Compile] $(BUILD)/obj/jst.o'
	$(CC) -c -o $(BUILD)/obj/jst.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/jst.c

#
#   mbedtls.o
#
DEPS_18 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/mbedtls.o: \
    src/mbedtls.c $(DEPS_18)
	@echo '   [Compile] $(BUILD)/obj/mbedtls.o'
	$(CC) -c -o $(BUILD)/obj/mbedtls.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/mbedtls.c

#
#   openssl.o
#
DEPS_19 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/openssl.o: \
    src/openssl.c $(DEPS_19)
	@echo '   [Compile] $(BUILD)/obj/openssl.o'
	$(CC) -c -o $(BUILD)/obj/openssl.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/openssl.c

#
#   options.o
#
DEPS_20 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/options.o: \
    src/options.c $(DEPS_20)
	@echo '   [Compile] $(BUILD)/obj/options.o'
	$(CC) -c -o $(BUILD)/obj/options.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/options.c

#
#   osdep.o
#
DEPS_21 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/osdep.o: \
    src/osdep.c $(DEPS_21)
	@echo '   [Compile] $(BUILD)/obj/osdep.o'
	$(CC) -c -o $(BUILD)/obj/osdep.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/osdep.c

#
#   rom.o
#
DEPS_22 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/rom.o: \
    src/rom.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/rom.o'
	$(CC) -c -o $(BUILD)/obj/rom.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/rom.c

#
#   route.o
#
DEPS_23 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/route.o: \
    src/route.c $(DEPS_23)
	@echo '   [Compile] $(BUILD)/obj/route.o'
	$(CC) -c -o $(BUILD)/obj/route.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/route.c

#
#   runtime.o
#
DEPS_24 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/runtime.o: \
    src/runtime.c $(DEPS_24)
	@echo '   [Compile] $(BUILD)/obj/runtime.o'
	$(CC) -c -o $(BUILD)/obj/runtime.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/runtime.c

#
#   socket.o
#
DEPS_25 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/socket.o: \
    src/socket.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/socket.o'
	$(CC) -c -o $(BUILD)/obj/socket.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/socket.c

#
#   test.o
#
DEPS_26 += $(BUILD)/inc/goahead.h
DEPS_26 += $(BUILD)/inc/js.h

$(BUILD)/obj/test.o: \
    test/test.c $(DEPS_26)
	@echo '   [Compile] $(BUILD)/obj/test.o'
	$(CC) -c -o $(BUILD)/obj/test.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) test/test.c

#
#   time.o
#
DEPS_27 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/time.o: \
    src/time.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/time.o'
	$(CC) -c -o $(BUILD)/obj/time.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/time.c

#
#   upload.o
#
DEPS_28 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/upload.o: \
    src/upload.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/upload.o'
	$(CC) -c -o $(BUILD)/obj/upload.o $(CFLAGS) $(DFLAGS) -D_FILE_OFFSET_BITS=64 $(IFLAGS) src/upload.c

#
#   libgo
#
DEPS_29 += $(BUILD)/inc/osdep.h
DEPS_29 += $(BUILD)/inc/goahead.h
DEPS_29 += $(BUILD)/inc/js.h
DEPS_29 += $(BUILD)/obj/action.o
DEPS_29 += $(BUILD)/obj/alloc.o
DEPS_29 += $(BUILD)/obj/auth.o
DEPS_29 += $(BUILD)/obj/cgi.o
DEPS_29 += $(BUILD)/obj/crypt.o
DEPS_29 += $(BUILD)/obj/file.o
DEPS_29 += $(BUILD)/obj/fs.o
DEPS_29 += $(BUILD)/obj/http.o
DEPS_29 += $(BUILD)/obj/js.o
DEPS_29 += $(BUILD)/obj/jst.o
DEPS_29 += $(BUILD)/obj/mbedtls.o
DEPS_29 += $(BUILD)/obj/openssl.o
DEPS_29 += $(BUILD)/obj/options.o
DEPS_29 += $(BUILD)/obj/osdep.o
DEPS_29 += $(BUILD)/obj/rom.o
DEPS_29 += $(BUILD)/obj/route.o
DEPS_29 += $(BUILD)/obj/runtime.o
DEPS_29 += $(BUILD)/obj/socket.o
DEPS_29 += $(BUILD)/obj/time.o
DEPS_29 += $(BUILD)/obj/upload.o

ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_29 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_29 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_29 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_29 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_29 += -lcrypto
endif

$(BUILD)/bin/libgo.so: $(DEPS_29)
	@echo '      [Link] $(BUILD)/bin/libgo.so'
	$(CC) -shared -o $(BUILD)/bin/libgo.so $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/action.o" "$(BUILD)/obj/alloc.o" "$(BUILD)/obj/auth.o" "$(BUILD)/obj/cgi.o" "$(BUILD)/obj/crypt.o" "$(BUILD)/obj/file.o" "$(BUILD)/obj/fs.o" "$(BUILD)/obj/http.o" "$(BUILD)/obj/js.o" "$(BUILD)/obj/jst.o" "$(BUILD)/obj/mbedtls.o" "$(BUILD)/obj/openssl.o" "$(BUILD)/obj/options.o" "$(BUILD)/obj/osdep.o" "$(BUILD)/obj/rom.o" "$(BUILD)/obj/route.o" "$(BUILD)/obj/runtime.o" "$(BUILD)/obj/socket.o" "$(BUILD)/obj/time.o" "$(BUILD)/obj/upload.o" $(LIBPATHS_29) $(LIBS_29) $(LIBS_29) $(LIBS) 

#
#   goahead
#
DEPS_30 += $(BUILD)/bin/libgo.so
DEPS_30 += $(BUILD)/inc/goahead.h
DEPS_30 += $(BUILD)/inc/js.h
DEPS_30 += $(BUILD)/obj/goahead.o

LIBS_30 += -lgo
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_30 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_30 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_30 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_30 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_30 += -lcrypto
endif

$(BUILD)/bin/goahead: $(DEPS_30)
	@echo '      [Link] $(BUILD)/bin/goahead'
	$(CC) -o $(BUILD)/bin/goahead $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/goahead.o" $(LIBPATHS_30) $(LIBS_30) $(LIBS_30) $(LIBS) $(LIBS) 

#
#   goahead-test
#
DEPS_31 += $(BUILD)/bin/libgo.so
DEPS_31 += $(BUILD)/obj/test.o

LIBS_31 += -lgo
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_31 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_31 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_31 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_31 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_31 += -lcrypto
endif

$(BUILD)/bin/goahead-test: $(DEPS_31)
	@echo '      [Link] $(BUILD)/bin/goahead-test'
	$(CC) -o $(BUILD)/bin/goahead-test $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/test.o" $(LIBPATHS_31) $(LIBS_31) $(LIBS_31) $(LIBS) $(LIBS) 

#
#   gopass
#
DEPS_32 += $(BUILD)/bin/libgo.so
DEPS_32 += $(BUILD)/inc/goahead.h
DEPS_32 += $(BUILD)/inc/js.h
DEPS_32 += $(BUILD)/obj/gopass.o

LIBS_32 += -lgo
ifeq ($(ME_COM_MBEDTLS),1)
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_32 += -lmbedtls
endif
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_32 += -lmbedcrypto
endif
ifeq ($(ME_COM_MBEDTLS),1)
    LIBS_32 += -lmbedx509
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_32 += -lssl
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_32 += -lcrypto
endif

$(BUILD)/bin/gopass: $(DEPS_32)
	@echo '      [Link] $(BUILD)/bin/gopass'
	$(CC) -o $(BUILD)/bin/gopass $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/gopass.o" $(LIBPATHS_32) $(LIBS_32) $(LIBS_32) $(LIBS) $(LIBS) 

#
#   install-roots
#
DEPS_33 += certs/roots.crt

$(BUILD)/.install-roots-modified: $(DEPS_33)
	@echo '      [Copy] $(BUILD)/bin'
	mkdir -p "$(BUILD)/bin"
	cp certs/roots.crt $(BUILD)/bin/roots.crt
	touch "$(BUILD)/.install-roots-modified" 2>/dev/null

#
#   stop
#

stop: $(DEPS_34)

#
#   installBinary
#

installBinary: $(DEPS_35)
	mkdir -p "$(ME_APP_PREFIX)" ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	ln -s "$(VERSION)" "$(ME_APP_PREFIX)/latest" ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	chmod 755 "$(ME_MAN_PREFIX)/man1" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	mkdir -p "$(ME_BIN_PREFIX)" ; \
	rm -f "$(ME_BIN_PREFIX)/goahead" ; \
	ln -s "$(ME_VAPP_PREFIX)/bin/goahead" "$(ME_BIN_PREFIX)/goahead" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp certs/roots.crt $(ME_VAPP_PREFIX)/bin/roots.crt ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp certs/self.crt $(ME_ETC_PREFIX)/self.crt ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp certs/self.key $(ME_ETC_PREFIX)/self.key ; \
	mkdir -p "$(ME_WEB_PREFIX)" ; \
	cp src/web/index.html $(ME_WEB_PREFIX)/index.html ; \
	cp src/web/favicon.ico $(ME_WEB_PREFIX)/favicon.ico ; \
	mkdir -p "$(ME_ETC_PREFIX)" ; \
	cp src/auth.txt $(ME_ETC_PREFIX)/auth.txt ; \
	cp src/route.txt $(ME_ETC_PREFIX)/route.txt ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/doc/man/man1" ; \
	cp doc/man/goahead.1 $(ME_VAPP_PREFIX)/doc/man/man1/goahead.1 ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	rm -f "$(ME_MAN_PREFIX)/man1/goahead.1" ; \
	ln -s "$(ME_VAPP_PREFIX)/doc/man/man1/goahead.1" "$(ME_MAN_PREFIX)/man1/goahead.1" ; \
	cp doc/man/gopass.1 $(ME_VAPP_PREFIX)/doc/man/man1/gopass.1 ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	rm -f "$(ME_MAN_PREFIX)/man1/gopass.1" ; \
	ln -s "$(ME_VAPP_PREFIX)/doc/man/man1/gopass.1" "$(ME_MAN_PREFIX)/man1/gopass.1" ; \
	cp doc/man/webcomp.1 $(ME_VAPP_PREFIX)/doc/man/man1/webcomp.1 ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	rm -f "$(ME_MAN_PREFIX)/man1/webcomp.1" ; \
	ln -s "$(ME_VAPP_PREFIX)/doc/man/man1/webcomp.1" "$(ME_MAN_PREFIX)/man1/webcomp.1"

#
#   start
#

start: $(DEPS_36)

#
#   install
#
DEPS_37 += stop
DEPS_37 += installBinary
DEPS_37 += start

install: $(DEPS_37)

#
#   installPrep
#

installPrep: $(DEPS_38)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with sudo." ; \
	exit 255 ; \
	fi

#
#   uninstall
#
DEPS_39 += stop

uninstall: $(DEPS_39)

#
#   uninstallBinary
#

uninstallBinary: $(DEPS_40)
	rm -fr "$(ME_WEB_PREFIX)" ; \
	rm -fr "$(ME_VAPP_PREFIX)" ; \
	rmdir -p "$(ME_ETC_PREFIX)" 2>/dev/null ; true ; \
	rmdir -p "$(ME_WEB_PREFIX)" 2>/dev/null ; true ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	rmdir -p "$(ME_APP_PREFIX)" 2>/dev/null ; true

#
#   version
#

version: $(DEPS_41)
	echo $(VERSION)


EXTRA_MAKEFILE := $(strip $(wildcard ./projects/extra.mk))
ifneq ($(EXTRA_MAKEFILE),)
include $(EXTRA_MAKEFILE)
endif
