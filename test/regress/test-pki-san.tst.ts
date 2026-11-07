/*
    test-pki-san.tst.ts - The test server certificates name the endpoints the suite connects to

    The test server listens on https://127.0.0.1 and https://[::1]. A client that verifies the server certificate
    matches an IP endpoint against the certificate's iPAddress subjectAltName entries only; a CN never matches an
    IP address, and current clients ignore the CN for DNS names too. Without subjectAltName the suite can reach
    the server only with verification off, and never exercises the certificate a verifying client would accept.
    This holds for the certificates copied from the certs pak and for the ones test/utils/pki.sh generates.
 */

import { ttrue } from '@embedthis/testme'
import { X509Certificate } from 'node:crypto'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const PKI = join(import.meta.dir, '..', 'pki')

//  The certificates the test server and the TLS tests present as server certificates
for (let name of ['self', 'test', 'ec']) {
    let path = join(PKI, `${name}.crt`)
    ttrue(existsSync(path), `test/pki/${name}.crt must exist`)
    if (!existsSync(path)) {
        continue
    }
    let cert = new X509Certificate(readFileSync(path))
    let san = cert.subjectAltName || '(none)'
    ttrue(cert.checkHost('localhost', { subject: 'never' }) == 'localhost',
        `test/pki/${name}.crt must name localhost in its subjectAltName, has ${san}`)
    ttrue(cert.checkIP('127.0.0.1') == '127.0.0.1',
        `test/pki/${name}.crt must name 127.0.0.1 in its subjectAltName, has ${san}`)
    ttrue(cert.checkIP('::1') == '::1',
        `test/pki/${name}.crt must name ::1 in its subjectAltName, has ${san}`)
}
