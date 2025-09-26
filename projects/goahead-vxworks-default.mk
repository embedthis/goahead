#
#   goahead-vxworks-default.mk -- Makefile to build Embedthis GoAhead for vxworks
#

NAME                  := goahead
VERSION               := 6.0.5
PROFILE               ?= default
ARCH                  ?= $(shell echo $(WIND_HOST_TYPE) | sed 's/-.*$(ME_ROOT_PREFIX)/')
CPU                   ?= $(subst X86,PENTIUM,$(shell echo $(ARCH) | tr a-z A-Z))
OS                    ?= vxworks
CC                    ?= cc$(subst x86,pentium,$(ARCH))
LD                    ?= ldundefined
AR                    ?= arundefined
BUILD                 ?= build/$(OS)-$(ARCH)-$(PROFILE)
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

#
# Components
#
ME_COM_COMPILER       ?= 1
ME_COM_LIB            ?= 1
ME_COM_LINK           ?= 1
ME_COM_MBEDTLS        ?= 0
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 0

ME_COM_OPENSSL_PATH   ?= "/path/to/openssl"

ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_LINK),1)
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
ME_COMPILER_HAS_ATOMIC ?= 0
ME_COMPILER_HAS_ATOMIC64 ?= 0
ME_COMPILER_HAS_DOUBLE_BRACES ?= 0
ME_COMPILER_HAS_DYN_LOAD ?= 1
ME_COMPILER_HAS_LIB_EDIT ?= 0
ME_COMPILER_HAS_LIB_RT ?= 0
ME_COMPILER_HAS_MMU   ?= 1
ME_COMPILER_HAS_MTUNE ?= 0
ME_COMPILER_HAS_PAM   ?= 0
ME_COMPILER_HAS_STACK_PROTECTOR ?= 1
ME_COMPILER_HAS_SYNC  ?= 0
ME_COMPILER_HAS_SYNC64 ?= 0
ME_COMPILER_HAS_SYNC_CAS ?= 0
ME_COMPILER_HAS_UNNAMED_UNIONS ?= 1
ME_COMPILER_WARN64TO32 ?= 0
ME_COMPILER_WARN_UNUSED ?= 0
ME_CONFIGURE          ?= \"me -d -q -platform vxworks-x86-default -configure . -gen make\"
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

export PATH           := $(WIND_GNU_PATH)/$(WIND_HOST_TYPE)/bin:$(PATH)
CFLAGS                += -fno-builtin -fno-defer-pop -fvolatile  -w
DFLAGS                += -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h" $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_LINK=$(ME_COM_LINK) -DME_COM_MBEDTLS=$(ME_COM_MBEDTLS) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_CERTS_GENDH=$(ME_CERTS_GENDH) -DME_GOAHEAD_ACCESSLOG=$(ME_GOAHEAD_ACCESSLOG) -DME_GOAHEAD_AUTH=$(ME_GOAHEAD_AUTH) -DME_GOAHEAD_AUTHSTORE=$(ME_GOAHEAD_AUTHSTORE) -DME_GOAHEAD_AUTOLOGIN=$(ME_GOAHEAD_AUTOLOGIN) -DME_GOAHEAD_CGI=$(ME_GOAHEAD_CGI) -DME_GOAHEAD_CGIVARPREFIX=$(ME_GOAHEAD_CGIVARPREFIX) -DME_GOAHEAD_CLIENTCACHE=$(ME_GOAHEAD_CLIENTCACHE) -DME_GOAHEAD_CLIENTCACHELIFESPAN=$(ME_GOAHEAD_CLIENTCACHELIFESPAN) -DME_GOAHEAD_DIGEST=$(ME_GOAHEAD_DIGEST) -DME_GOAHEAD_DOCUMENTS=$(ME_GOAHEAD_DOCUMENTS) -DME_GOAHEAD_JAVASCRIPT=$(ME_GOAHEAD_JAVASCRIPT) -DME_GOAHEAD_LEGACY=$(ME_GOAHEAD_LEGACY) -DME_GOAHEAD_LIMITBUFFER=$(ME_GOAHEAD_LIMITBUFFER) -DME_GOAHEAD_LIMITCGIARGS=$(ME_GOAHEAD_LIMITCGIARGS) -DME_GOAHEAD_LIMITFILENAME=$(ME_GOAHEAD_LIMITFILENAME) -DME_GOAHEAD_LIMITFILES=$(ME_GOAHEAD_LIMITFILES) -DME_GOAHEAD_LIMITHEADER=$(ME_GOAHEAD_LIMITHEADER) -DME_GOAHEAD_LIMITHEADERS=$(ME_GOAHEAD_LIMITHEADERS) -DME_GOAHEAD_LIMITNUMHEADERS=$(ME_GOAHEAD_LIMITNUMHEADERS) -DME_GOAHEAD_LIMITPARSETIMEOUT=$(ME_GOAHEAD_LIMITPARSETIMEOUT) -DME_GOAHEAD_LIMITPASSWORD=$(ME_GOAHEAD_LIMITPASSWORD) -DME_GOAHEAD_LIMITPOST=$(ME_GOAHEAD_LIMITPOST) -DME_GOAHEAD_LIMITPUT=$(ME_GOAHEAD_LIMITPUT) -DME_GOAHEAD_LIMITSESSIONCOUNT=$(ME_GOAHEAD_LIMITSESSIONCOUNT) -DME_GOAHEAD_LIMITSESSIONLIFE=$(ME_GOAHEAD_LIMITSESSIONLIFE) -DME_GOAHEAD_LIMITSTRING=$(ME_GOAHEAD_LIMITSTRING) -DME_GOAHEAD_LIMITTIMEOUT=$(ME_GOAHEAD_LIMITTIMEOUT) -DME_GOAHEAD_LIMITUPLOAD=$(ME_GOAHEAD_LIMITUPLOAD) -DME_GOAHEAD_LIMITURI=$(ME_GOAHEAD_LIMITURI) -DME_GOAHEAD_LISTEN=$(ME_GOAHEAD_LISTEN) -DME_GOAHEAD_LOGFILE=$(ME_GOAHEAD_LOGFILE) -DME_GOAHEAD_LOGGING=$(ME_GOAHEAD_LOGGING) -DME_GOAHEAD_PUTDIR=$(ME_GOAHEAD_PUTDIR) -DME_GOAHEAD_REALM=$(ME_GOAHEAD_REALM) -DME_GOAHEAD_REPLACE_MALLOC=$(ME_GOAHEAD_REPLACE_MALLOC) -DME_GOAHEAD_SSL=$(ME_GOAHEAD_SSL) -DME_GOAHEAD_STEALTH=$(ME_GOAHEAD_STEALTH) -DME_GOAHEAD_TRACING=$(ME_GOAHEAD_TRACING) -DME_GOAHEAD_UPLOAD=$(ME_GOAHEAD_UPLOAD) -DME_GOAHEAD_UPLOADDIR=$(ME_GOAHEAD_UPLOADDIR) -DME_GOAHEAD_XFRAMEHEADER=$(ME_GOAHEAD_XFRAMEHEADER) -DME_MBEDTLS_COMPACT=$(ME_MBEDTLS_COMPACT) -DME_OPENSSL_VERSION=$(ME_OPENSSL_VERSION) 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += -Wl,-r
LIBPATHS              += -L$(BUILD)/bin
LIBS                  += -lgcc

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

ME_ROOT_PREFIX        ?= deploy
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)
ME_DATA_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_STATE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_BIN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_INC_PREFIX         ?= $(ME_VAPP_PREFIX)/inc
ME_LIB_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_MAN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SBIN_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_ETC_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_WEB_PREFIX         ?= $(ME_VAPP_PREFIX)/web
ME_LOG_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SPOOL_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_CACHE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/src/$(NAME)-$(VERSION)


TARGETS               += $(BUILD)/bin/goahead.out
TARGETS               += $(BUILD)/bin/goahead-test.out
TARGETS               += $(BUILD)/bin/gopass.out
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
	@if [ "$(WIND_BASE)" = "" ] ; then echo WARNING: WIND_BASE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_HOST_TYPE)" = "" ] ; then echo WARNING: WIND_HOST_TYPE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_GNU_PATH)" = "" ] ; then echo WARNING: WIND_GNU_PATH not set. Run wrenv.sh. ; exit 255 ; fi
	@[ ! -x $(BUILD)/bin ] && mkdir -p $(BUILD)/bin; true
	@[ ! -x $(BUILD)/inc ] && mkdir -p $(BUILD)/inc; true
	@[ ! -x $(BUILD)/obj ] && mkdir -p $(BUILD)/obj; true
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/goahead-vxworks-$(PROFILE)-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/goahead-vxworks-$(PROFILE)-me.h >/dev/null ; then\
		cp projects/goahead-vxworks-$(PROFILE)-me.h $(BUILD)/inc/me.h  ; \
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
	rm -f "$(BUILD)/bin/goahead.out"
	rm -f "$(BUILD)/bin/goahead-test.out"
	rm -f "$(BUILD)/bin/gopass.out"
	rm -f "$(BUILD)/.install-roots-modified"
	rm -f "$(BUILD)/bin/libgo.out"

clobber: clean
	rm -fr ./$(BUILD)

#
#   esp_crt_bundle.h
#

$(BUILD)/inc/esp_crt_bundle.h: $(DEPS_1)

#
#   esp_err.h
#

$(BUILD)/inc/esp_err.h: $(DEPS_2)

#
#   esp_event.h
#

$(BUILD)/inc/esp_event.h: $(DEPS_3)

#
#   esp_heap_caps.h
#

$(BUILD)/inc/esp_heap_caps.h: $(DEPS_4)

#
#   esp_littlefs.h
#

$(BUILD)/inc/esp_littlefs.h: $(DEPS_5)

#
#   esp_log.h
#

$(BUILD)/inc/esp_log.h: $(DEPS_6)

#
#   esp_netif.h
#

$(BUILD)/inc/esp_netif.h: $(DEPS_7)

#
#   esp_psram.h
#

$(BUILD)/inc/esp_psram.h: $(DEPS_8)

#
#   esp_pthread.h
#

$(BUILD)/inc/esp_pthread.h: $(DEPS_9)

#
#   esp_system.h
#

$(BUILD)/inc/esp_system.h: $(DEPS_10)

#
#   esp_wifi.h
#

$(BUILD)/inc/esp_wifi.h: $(DEPS_11)

#
#   FreeRTOS.h
#

$(BUILD)/inc/freertos/FreeRTOS.h: $(DEPS_12)

#
#   event_groups.h
#

$(BUILD)/inc/freertos/event_groups.h: $(DEPS_13)

#
#   task.h
#

$(BUILD)/inc/freertos/task.h: $(DEPS_14)

#
#   me.h
#

$(BUILD)/inc/me.h: $(DEPS_15)

#
#   time.h
#

$(BUILD)/inc/time.h: $(DEPS_16)

#
#   nvs_flash.h
#

$(BUILD)/inc/nvs_flash.h: $(DEPS_17)

#
#   err.h
#

$(BUILD)/inc/lwip/err.h: $(DEPS_18)

#
#   sockets.h
#

$(BUILD)/inc/lwip/sockets.h: $(DEPS_19)

#
#   sys.h
#

$(BUILD)/inc/lwip/sys.h: $(DEPS_20)

#
#   netdb.h
#

$(BUILD)/inc/lwip/netdb.h: $(DEPS_21)

#
#   osdep.h
#
DEPS_22 += src/osdep/osdep.h
DEPS_22 += $(BUILD)/inc/me.h
DEPS_22 += $(BUILD)/inc/freertos/FreeRTOS.h
DEPS_22 += $(BUILD)/inc/freertos/event_groups.h
DEPS_22 += $(BUILD)/inc/freertos/task.h
DEPS_22 += $(BUILD)/inc/time.h
DEPS_22 += $(BUILD)/inc/esp_system.h
DEPS_22 += $(BUILD)/inc/esp_log.h
DEPS_22 += $(BUILD)/inc/esp_heap_caps.h
DEPS_22 += $(BUILD)/inc/esp_err.h
DEPS_22 += $(BUILD)/inc/esp_event.h
DEPS_22 += $(BUILD)/inc/esp_psram.h
DEPS_22 += $(BUILD)/inc/esp_pthread.h
DEPS_22 += $(BUILD)/inc/esp_littlefs.h
DEPS_22 += $(BUILD)/inc/esp_crt_bundle.h
DEPS_22 += $(BUILD)/inc/esp_wifi.h
DEPS_22 += $(BUILD)/inc/esp_netif.h
DEPS_22 += $(BUILD)/inc/nvs_flash.h
DEPS_22 += $(BUILD)/inc/lwip/err.h
DEPS_22 += $(BUILD)/inc/lwip/sockets.h
DEPS_22 += $(BUILD)/inc/lwip/sys.h
DEPS_22 += $(BUILD)/inc/lwip/netdb.h

$(BUILD)/inc/osdep.h: $(DEPS_22)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp src/osdep/osdep.h $(BUILD)/inc/osdep.h

#
#   goahead.h
#
DEPS_23 += src/goahead.h
DEPS_23 += $(BUILD)/inc/me.h
DEPS_23 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/goahead.h: $(DEPS_23)
	@echo '      [Copy] $(BUILD)/inc/goahead.h'
	mkdir -p "$(BUILD)/inc"
	cp src/goahead.h $(BUILD)/inc/goahead.h

#
#   js.h
#
DEPS_24 += src/js.h
DEPS_24 += $(BUILD)/inc/goahead.h

$(BUILD)/inc/js.h: $(DEPS_24)
	@echo '      [Copy] $(BUILD)/inc/js.h'
	mkdir -p "$(BUILD)/inc"
	cp src/js.h $(BUILD)/inc/js.h

#
#   action.o
#
DEPS_25 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/action.o: \
    src/action.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/action.o'
	$(CC) -c -o $(BUILD)/obj/action.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/action.c

#
#   alloc.o
#
DEPS_26 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/alloc.o: \
    src/alloc.c $(DEPS_26)
	@echo '   [Compile] $(BUILD)/obj/alloc.o'
	$(CC) -c -o $(BUILD)/obj/alloc.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/alloc.c

#
#   auth.o
#
DEPS_27 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/auth.o: \
    src/auth.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/auth.o'
	$(CC) -c -o $(BUILD)/obj/auth.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/auth.c

#
#   cgi.o
#
DEPS_28 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/cgi.o: \
    src/cgi.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/cgi.o'
	$(CC) -c -o $(BUILD)/obj/cgi.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/cgi.c

#
#   cgitest.o
#

$(BUILD)/obj/cgitest.o: \
    test/cgitest.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/cgitest.o'
	$(CC) -c -o $(BUILD)/obj/cgitest.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" $(IFLAGS) test/cgitest.c

#
#   crypt.o
#
DEPS_30 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/crypt.o: \
    src/crypt.c $(DEPS_30)
	@echo '   [Compile] $(BUILD)/obj/crypt.o'
	$(CC) -c -o $(BUILD)/obj/crypt.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/crypt.c

#
#   file.o
#
DEPS_31 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/file.o: \
    src/file.c $(DEPS_31)
	@echo '   [Compile] $(BUILD)/obj/file.o'
	$(CC) -c -o $(BUILD)/obj/file.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/file.c

#
#   fs.o
#
DEPS_32 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/fs.o: \
    src/fs.c $(DEPS_32)
	@echo '   [Compile] $(BUILD)/obj/fs.o'
	$(CC) -c -o $(BUILD)/obj/fs.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/fs.c

#
#   goahead.o
#
DEPS_33 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/goahead.o: \
    src/goahead.c $(DEPS_33)
	@echo '   [Compile] $(BUILD)/obj/goahead.o'
	$(CC) -c -o $(BUILD)/obj/goahead.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/goahead.c

#
#   gopass.o
#
DEPS_34 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/gopass.o: \
    src/utils/gopass.c $(DEPS_34)
	@echo '   [Compile] $(BUILD)/obj/gopass.o'
	$(CC) -c -o $(BUILD)/obj/gopass.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/utils/gopass.c

#
#   http.o
#
DEPS_35 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/http.o: \
    src/http.c $(DEPS_35)
	@echo '   [Compile] $(BUILD)/obj/http.o'
	$(CC) -c -o $(BUILD)/obj/http.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/http.c

#
#   js.o
#
DEPS_36 += $(BUILD)/inc/js.h

$(BUILD)/obj/js.o: \
    src/js.c $(DEPS_36)
	@echo '   [Compile] $(BUILD)/obj/js.o'
	$(CC) -c -o $(BUILD)/obj/js.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/js.c

#
#   jst.o
#
DEPS_37 += $(BUILD)/inc/goahead.h
DEPS_37 += $(BUILD)/inc/js.h

$(BUILD)/obj/jst.o: \
    src/jst.c $(DEPS_37)
	@echo '   [Compile] $(BUILD)/obj/jst.o'
	$(CC) -c -o $(BUILD)/obj/jst.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/jst.c

#
#   mbedtls.o
#
DEPS_38 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/mbedtls.o: \
    src/mbedtls.c $(DEPS_38)
	@echo '   [Compile] $(BUILD)/obj/mbedtls.o'
	$(CC) -c -o $(BUILD)/obj/mbedtls.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/mbedtls.c

#
#   openssl.o
#
DEPS_39 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/openssl.o: \
    src/openssl.c $(DEPS_39)
	@echo '   [Compile] $(BUILD)/obj/openssl.o'
	$(CC) -c -o $(BUILD)/obj/openssl.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/openssl.c

#
#   options.o
#
DEPS_40 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/options.o: \
    src/options.c $(DEPS_40)
	@echo '   [Compile] $(BUILD)/obj/options.o'
	$(CC) -c -o $(BUILD)/obj/options.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/options.c

#
#   osdep.o
#
DEPS_41 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/osdep.o: \
    src/osdep.c $(DEPS_41)
	@echo '   [Compile] $(BUILD)/obj/osdep.o'
	$(CC) -c -o $(BUILD)/obj/osdep.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/osdep.c

#
#   rom.o
#
DEPS_42 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/rom.o: \
    src/rom.c $(DEPS_42)
	@echo '   [Compile] $(BUILD)/obj/rom.o'
	$(CC) -c -o $(BUILD)/obj/rom.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/rom.c

#
#   route.o
#
DEPS_43 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/route.o: \
    src/route.c $(DEPS_43)
	@echo '   [Compile] $(BUILD)/obj/route.o'
	$(CC) -c -o $(BUILD)/obj/route.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/route.c

#
#   runtime.o
#
DEPS_44 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/runtime.o: \
    src/runtime.c $(DEPS_44)
	@echo '   [Compile] $(BUILD)/obj/runtime.o'
	$(CC) -c -o $(BUILD)/obj/runtime.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/runtime.c

#
#   socket.o
#
DEPS_45 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/socket.o: \
    src/socket.c $(DEPS_45)
	@echo '   [Compile] $(BUILD)/obj/socket.o'
	$(CC) -c -o $(BUILD)/obj/socket.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/socket.c

#
#   test.o
#
DEPS_46 += $(BUILD)/inc/goahead.h
DEPS_46 += $(BUILD)/inc/js.h

$(BUILD)/obj/test.o: \
    test/test.c $(DEPS_46)
	@echo '   [Compile] $(BUILD)/obj/test.o'
	$(CC) -c -o $(BUILD)/obj/test.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" test/test.c

#
#   time.o
#
DEPS_47 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/time.o: \
    src/time.c $(DEPS_47)
	@echo '   [Compile] $(BUILD)/obj/time.o'
	$(CC) -c -o $(BUILD)/obj/time.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/time.c

#
#   upload.o
#
DEPS_48 += $(BUILD)/inc/goahead.h

$(BUILD)/obj/upload.o: \
    src/upload.c $(DEPS_48)
	@echo '   [Compile] $(BUILD)/obj/upload.o'
	$(CC) -c -o $(BUILD)/obj/upload.o $(CFLAGS) -DME_DEBUG=1 -DVXWORKS -DRW_MULTI_THREAD -DCPU=PENTIUM -DTOOL_FAMILY=gnu -DTOOL=gnu -D_GNU_TOOL -D_WRS_KERNEL_ -D_VSB_CONFIG_FILE=\"/WindRiver/vxworks-7/samples/prebuilt_projects/vsb_vxsim_linux/h/config/vsbConfig.h\" -DME_COM_OPENSSL_PATH=$(ME_COM_OPENSSL_PATH) -D_FILE_OFFSET_BITS=64 $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/upload.c

#
#   libgo
#
DEPS_49 += $(BUILD)/inc/osdep.h
DEPS_49 += $(BUILD)/inc/goahead.h
DEPS_49 += $(BUILD)/inc/js.h
DEPS_49 += $(BUILD)/obj/action.o
DEPS_49 += $(BUILD)/obj/alloc.o
DEPS_49 += $(BUILD)/obj/auth.o
DEPS_49 += $(BUILD)/obj/cgi.o
DEPS_49 += $(BUILD)/obj/crypt.o
DEPS_49 += $(BUILD)/obj/file.o
DEPS_49 += $(BUILD)/obj/fs.o
DEPS_49 += $(BUILD)/obj/http.o
DEPS_49 += $(BUILD)/obj/js.o
DEPS_49 += $(BUILD)/obj/jst.o
DEPS_49 += $(BUILD)/obj/mbedtls.o
DEPS_49 += $(BUILD)/obj/openssl.o
DEPS_49 += $(BUILD)/obj/options.o
DEPS_49 += $(BUILD)/obj/osdep.o
DEPS_49 += $(BUILD)/obj/rom.o
DEPS_49 += $(BUILD)/obj/route.o
DEPS_49 += $(BUILD)/obj/runtime.o
DEPS_49 += $(BUILD)/obj/socket.o
DEPS_49 += $(BUILD)/obj/time.o
DEPS_49 += $(BUILD)/obj/upload.o

ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_49 += -lssl
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_49 += -lcrypto
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/libgo.out: $(DEPS_49)
	@echo '      [Link] $(BUILD)/bin/libgo.out'
	$(CC) -r -o $(BUILD)/bin/libgo.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/action.o" "$(BUILD)/obj/alloc.o" "$(BUILD)/obj/auth.o" "$(BUILD)/obj/cgi.o" "$(BUILD)/obj/crypt.o" "$(BUILD)/obj/file.o" "$(BUILD)/obj/fs.o" "$(BUILD)/obj/http.o" "$(BUILD)/obj/js.o" "$(BUILD)/obj/jst.o" "$(BUILD)/obj/mbedtls.o" "$(BUILD)/obj/openssl.o" "$(BUILD)/obj/options.o" "$(BUILD)/obj/osdep.o" "$(BUILD)/obj/rom.o" "$(BUILD)/obj/route.o" "$(BUILD)/obj/runtime.o" "$(BUILD)/obj/socket.o" "$(BUILD)/obj/time.o" "$(BUILD)/obj/upload.o" $(LIBPATHS_49) $(LIBS_49) $(LIBS_49) $(LIBS) 

#
#   goahead
#
DEPS_50 += $(BUILD)/bin/libgo.out
DEPS_50 += $(BUILD)/inc/goahead.h
DEPS_50 += $(BUILD)/inc/js.h
DEPS_50 += $(BUILD)/obj/goahead.o

ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_50 += -lssl
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_50 += -lcrypto
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/goahead.out: $(DEPS_50)
	@echo '      [Link] $(BUILD)/bin/goahead.out'
	$(CC) -o $(BUILD)/bin/goahead.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/goahead.o" $(LIBPATHS_50) $(LIBS_50) $(LIBS_50) $(LIBS) -Wl,-r 

#
#   goahead-test
#
DEPS_51 += $(BUILD)/bin/libgo.out
DEPS_51 += $(BUILD)/obj/test.o

ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_51 += -lssl
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_51 += -lcrypto
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/goahead-test.out: $(DEPS_51)
	@echo '      [Link] $(BUILD)/bin/goahead-test.out'
	$(CC) -o $(BUILD)/bin/goahead-test.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/test.o" $(LIBPATHS_51) $(LIBS_51) $(LIBS_51) $(LIBS) -Wl,-r 

#
#   gopass
#
DEPS_52 += $(BUILD)/bin/libgo.out
DEPS_52 += $(BUILD)/inc/goahead.h
DEPS_52 += $(BUILD)/inc/js.h
DEPS_52 += $(BUILD)/obj/gopass.o

ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_52 += -lssl
    LIBPATHS_52 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_52 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_52 += -lcrypto
    LIBPATHS_52 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_52 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/gopass.out: $(DEPS_52)
	@echo '      [Link] $(BUILD)/bin/gopass.out'
	$(CC) -o $(BUILD)/bin/gopass.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/gopass.o" $(LIBPATHS_52) $(LIBS_52) $(LIBS_52) $(LIBS) -Wl,-r 

#
#   install-roots
#
DEPS_53 += certs/roots.crt

$(BUILD)/.install-roots-modified: $(DEPS_53)
	@echo '      [Copy] $(BUILD)/bin'
	mkdir -p "$(BUILD)/bin"
	cp certs/roots.crt $(BUILD)/bin/roots.crt
	touch "$(BUILD)/.install-roots-modified" 2>/dev/null

#
#   stop
#

stop: $(DEPS_54)

#
#   installBinary
#

installBinary: $(DEPS_55)

#
#   start
#

start: $(DEPS_56)

#
#   install
#
DEPS_57 += stop
DEPS_57 += installBinary
DEPS_57 += start

install: $(DEPS_57)

#
#   installPrep
#

installPrep: $(DEPS_58)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with sudo." ; \
	exit 255 ; \
	fi

#
#   uninstall
#
DEPS_59 += stop

uninstall: $(DEPS_59)

#
#   uninstallBinary
#

uninstallBinary: $(DEPS_60)

#
#   version
#

version: $(DEPS_61)
	echo $(VERSION)


EXTRA_MAKEFILE := $(strip $(wildcard ./projects/extra.mk))
ifneq ($(EXTRA_MAKEFILE),)
include $(EXTRA_MAKEFILE)
endif
