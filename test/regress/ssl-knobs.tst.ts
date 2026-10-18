/*
    ssl-knobs.tst - Every TLS setting declared in goahead.h is read by a backend
 */

import { ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const SRC = fileURLToPath(new URL('../../src/', import.meta.url))
const ONE_BACKEND: Record<string, string> = {
    ME_GOAHEAD_SSL_HANDSHAKES: 'openssl.c',     // MbedTLS never renegotiates
    ME_GOAHEAD_SSL_LOG_LEVEL: 'mbedtls.c',      // OpenSSL has no debug trace
}

let header = readFileSync(SRC + 'goahead.h', 'utf8')
let backends: Record<string, string> = {
    'openssl.c': readFileSync(SRC + 'openssl.c', 'utf8'),
    'mbedtls.c': readFileSync(SRC + 'mbedtls.c', 'utf8'),
}
let settings = [...new Set([...header.matchAll(/#define (ME_GOAHEAD_SSL_\w+)/g)].map(m => m[1]))]
ttrue(settings.length >= 10, `expected the TLS settings in goahead.h, found ${settings.length}`)

for (let name of settings) {
    let pattern = new RegExp(`\\b${name}\\b`)
    let readers = Object.keys(backends).filter(file => pattern.test(backends[file]))
    if (ONE_BACKEND[name]) {
        ttrue(readers.includes(ONE_BACKEND[name]), `${name} must be read by ${ONE_BACKEND[name]}`)
    } else {
        ttrue(readers.length == 2, `${name} must be read by both backends, read by [${readers}]`)
    }
}
