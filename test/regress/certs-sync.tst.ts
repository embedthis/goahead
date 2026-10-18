/*
    certs-sync.tst.ts - certs/ is an exact export of the certs pak and the test certificates in it are current

    The test PKI (test/utils/pki.sh) copies its certificates from certs/ in the repository. A source distribution
    has no certs/: pki.sh generates the PKI there.
 */

import { tinfo, ttrue } from '@embedthis/testme'
import { X509Certificate } from 'node:crypto'
import { existsSync, readFileSync, readdirSync } from 'node:fs'
import { join } from 'node:path'

const TOP = join(import.meta.dir, '..', '..')
const CERTS = join(TOP, 'certs')
const PAK = join(TOP, 'paks', 'certs', 'dist')
const YEAR = 365 * 24 * 3600 * 1000

if (!existsSync(CERTS)) {
    tinfo('certs/ is not present, as in a source distribution. The test PKI is generated instead.')
} else {
    if (existsSync(PAK)) {
        let pakFiles = readdirSync(PAK).sort()
        let localFiles = readdirSync(CERTS).filter(name => name != '.DS_Store').sort()
        let missing = pakFiles.filter(name => !localFiles.includes(name))
        let extra = localFiles.filter(name => !pakFiles.includes(name))
        let differ = pakFiles.filter(name => localFiles.includes(name) &&
            !readFileSync(join(PAK, name)).equals(readFileSync(join(CERTS, name))))
        if (missing.length || extra.length || differ.length) {
            tinfo('Refresh certs/ from the pak: remove certs/ and run "pak sync"')
        }
        ttrue(missing.length == 0, `certs/ must hold every file in the certs pak, missing: ${missing.join(', ')}`)
        ttrue(extra.length == 0, `certs/ must hold only files from the certs pak, extra: ${extra.join(', ')}`)
        ttrue(differ.length == 0, `certs/ must match the certs pak byte for byte, differ: ${differ.join(', ')}`)
    } else {
        tinfo('paks/certs is not installed, so certs/ is not compared with the pak. Run "pak install" to compare.')
    }

    //  The certificates test/utils/pki.sh copies into the test PKI
    for (let name of ['ca.crt', 'ec.crt', 'self.crt', 'test.crt']) {
        let path = join(CERTS, name)
        ttrue(existsSync(path), `test certificate certs/${name} must exist`)
        if (!existsSync(path)) {
            continue
        }
        let cert = new X509Certificate(readFileSync(path))
        let expires = new Date(cert.validTo)
        ttrue(expires.getTime() - Date.now() >= YEAR,
            `test certificate certs/${name} (expires ${cert.validTo}) must be valid for at least a year`)
    }
}
