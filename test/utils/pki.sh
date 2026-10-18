#!/bin/bash
#
#   pki.sh - Populate a directory with the test PKI the TLS tests read
#
#   usage: pki.sh DIR
#
#   DIR receives four certificates and their keys:
#       ca      a certificate authority
#       test    an RSA server and client certificate issued by ca
#       self    a self-signed RSA certificate
#       ec      a self-signed P-256 certificate
#
#   In the repository these are copied from certs/, the certs pak export. A source distribution ships no private
#   key, so there they are generated afresh on every run.
#
DIR="$1"
CERTS="$(cd "$(dirname "$0")/../.." && pwd)/certs"
NAMES="ca self test ec"
DAYS=365

if [ -z "$DIR" ] ; then
    echo "usage: pki.sh DIR" >&2
    exit 1
fi
rm -rf "$DIR"
mkdir -p "$DIR"

complete=1
for name in $NAMES ; do
    if [ ! -f "$CERTS/$name.crt" ] || [ ! -f "$CERTS/$name.key" ] ; then
        complete=0
    fi
done
if [ $complete = 1 ] ; then
    for name in $NAMES ; do
        cp "$CERTS/$name.crt" "$CERTS/$name.key" "$DIR/"
    done
    exit 0
fi

if ! command -v openssl >/dev/null 2>&1 ; then
    echo "      [Error] openssl is required to generate the test PKI for the TLS tests" >&2
    exit 1
fi
echo "      [Run] generate the test PKI in ${DIR}"

ssl() {
    local out
    if ! out=$(cd "$DIR" && openssl "$@" 2>&1 < /dev/null) ; then
        echo "      [Error] openssl $1 failed: ${out}" >&2
        exit 1
    fi
}

#
#   Each certificate's subject and extensions are in its own configuration file. A subject given with -subj
#   ("/O=...") is rewritten as a file path by Git Bash on Windows.
#
conf() {
    local name="$1" ou="$2" cn="$3" ext="$4"
    {
        printf '[req]\nprompt = no\ndistinguished_name = dn\n[dn]\nO = GoAhead Test\n'
        [ -n "$ou" ] && printf 'OU = %s\n' "$ou"
        printf 'CN = %s\n[ext]\n%s\n' "$cn" "$ext"
    } > "$DIR/$name.cnf"
}
CA_EXT='basicConstraints = critical, CA:TRUE
keyUsage = critical, keyCertSign, cRLSign, digitalSignature
subjectKeyIdentifier = hash'
SELF_EXT='basicConstraints = critical, CA:TRUE
subjectKeyIdentifier = hash'
LEAF_EXT='basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth, clientAuth
subjectAltName = DNS:localhost, IP:127.0.0.1
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid'

conf ca "" "GoAhead Test CA" "$CA_EXT"
ssl genrsa -out ca.key 2048
ssl req -new -x509 -sha256 -days $DAYS -config ca.cnf -extensions ext -key ca.key -out ca.crt

conf test Test localhost "$LEAF_EXT"
ssl genrsa -out test.key 2048
ssl req -new -sha256 -config test.cnf -key test.key -out test.csr
ssl x509 -req -sha256 -days $DAYS -in test.csr -CA ca.crt -CAkey ca.key -set_serial 2 \
    -extfile test.cnf -extensions ext -out test.crt

conf self Self localhost "$SELF_EXT"
ssl genrsa -out self.key 2048
ssl req -new -x509 -sha256 -days $DAYS -config self.cnf -extensions ext -key self.key -out self.crt

conf ec EC localhost "$SELF_EXT"
ssl ecparam -genkey -name prime256v1 -out ec.key
ssl req -new -x509 -sha256 -days $DAYS -config ec.cnf -extensions ext -key ec.key -out ec.crt

rm -f "$DIR"/*.cnf "$DIR/test.csr"
exit 0
