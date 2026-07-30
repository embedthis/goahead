#
#   Makefile -- Test certificate generation
#
#   Generates SSL/TLS test certificates using OpenSSL.
#   WARNING: All certificates are for testing only -- never use in production.
#
#   Use "make help" for available targets and options.
#

NAME    := certs
SRC     := src
DIST    := dist
BITS    := 2048
DAYS    := 3650

.PHONY: all build ca-cert self-signed-cert test-cert ec-cert cert-request \
        show-certs roots package cache clean help

ifndef SHOW
.SILENT:
endif

all: build

build: ca-cert self-signed-cert test-cert ec-cert
	rm -f $(DIST)/*.old $(DIST)/*.csr $(DIST)/ca.db $(DIST)/ca.db.attr $(DIST)/*.pem $(DIST)/ca.srl
	@echo "      [Info] All test certificates generated in $(DIST)/"

ca-cert:
	@echo "      [Run] CA Certificate: $(DIST)/ca.crt"
	mkdir -p $(DIST)
	openssl genrsa -out $(DIST)/ca.key $(BITS) 2>/dev/null
	openssl req -config $(SRC)/openssl.conf -new -x509 -days $(DAYS) \
		-key $(DIST)/ca.key -out $(DIST)/ca.crt -extensions caExtensions
	printf '9999991000\n' > $(DIST)/ca.srl
	touch $(DIST)/ca.db

self-signed-cert:
	@echo "      [Run] Self-signed Certificate: $(DIST)/self.crt"
	mkdir -p $(DIST)
	openssl genrsa -out $(DIST)/self.key $(BITS) 2>/dev/null
	openssl req -new -x509 -days $(DAYS) -key $(DIST)/self.key -out $(DIST)/self.crt \
		< $(SRC)/self.ans

test-cert: ca-cert
	@echo "      [Run] Test Certificate: $(DIST)/test.crt"
	mkdir -p $(DIST)
	openssl genrsa -out $(DIST)/test.key $(BITS) 2>/dev/null
	openssl req -new -key $(DIST)/test.key -out $(DIST)/test.csr < $(SRC)/test.ans
	openssl ca -batch -config $(SRC)/openssl.conf -notext \
		-in $(DIST)/test.csr -out $(DIST)/test.crt -extensions server

ec-cert:
	@echo "      [Run] EC Certificate: $(DIST)/ec.crt"
	mkdir -p $(DIST)
	openssl ecparam -genkey -name prime256v1 -out $(DIST)/ec.key 2>/dev/null
	openssl req -new -key $(DIST)/ec.key -out $(DIST)/ec.csr < $(SRC)/ec.ans
	openssl req -x509 -days $(DAYS) -key $(DIST)/ec.key -in $(DIST)/ec.csr -out $(DIST)/ec.crt

cert-request:
	@echo "      [Run] Certificate Request: $(DIST)/server.csr"
	mkdir -p $(DIST)
	openssl genrsa -out $(DIST)/server.key $(BITS) 2>/dev/null
	openssl req -new -key $(DIST)/server.key -out $(DIST)/server.csr < $(SRC)/test.ans

show-certs:
	openssl x509 -in $(DIST)/ca.crt -noout -text
	openssl x509 -in $(DIST)/test.crt -noout -text
	openssl x509 -in $(DIST)/self.crt -noout -text
	openssl x509 -in $(DIST)/ec.crt -noout -text

roots:
	@echo "      [Run] Download Mozilla root CA bundle"
	mkdir -p $(DIST)
	curl -s -o $(DIST)/roots.crt https://curl.se/ca/cacert.pem

package:
	@echo "      [Run] Package dist/"
	mkdir -p $(DIST)
	cp $(SRC)/*.ans $(DIST)/
	cp $(SRC)/openssl.conf $(DIST)/
	cp $(SRC)/aws.crt $(DIST)/
	cp $(SRC)/roots-min.crt $(DIST)/
	cp CLAUDE.md $(DIST)/
	cp README.md $(DIST)/
	cp Makefile $(DIST)/

cache: build doc package
	cache

clean:
	@echo "      [Run] clean"
	rm -rf $(DIST)/

help:
	@echo '' >&2
	@echo 'usage: make [target]' >&2
	@echo '' >&2
	@echo 'Targets:' >&2
	@echo '  build               Generate all test certificates (default)' >&2
	@echo '  ca-cert             Generate CA certificate (ca.crt, ca.key)' >&2
	@echo '  self-signed-cert    Generate self-signed certificate (self.crt, self.key)' >&2
	@echo '  test-cert           Generate CA-signed test certificate (test.crt, test.key)' >&2
	@echo '  ec-cert             Generate EC certificate (ec.crt, ec.key)' >&2
	@echo '  cert-request        Generate CSR for external CA (server.csr, server.key)' >&2
	@echo '  show-certs          Display all certificate details' >&2
	@echo '  roots               Download Mozilla root CA bundle' >&2
	@echo '  package             Copy source files to dist/' >&2
	@echo '  cache               Build, package and cache' >&2
	@echo '  clean               Remove all generated files' >&2
	@echo '' >&2
	@echo 'Make variables:' >&2
	@echo '  SHOW=1              Show build commands' >&2
	@echo '' >&2
