/*
    test-pki.tst.ts - Without the certs pak's keys, test prep generates a complete test PKI, and the tests read
    their certificates and keys from it alone
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { copyFileSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const TEST = join(import.meta.dir, '..')
const NAMES = ['ca', 'self', 'test', 'ec']

/*
    The TLS tests must not read certs/: a source distribution ships no private key there
 */
function checkSources() {
    const self = import.meta.path.split('/').pop()
    const pattern = /certs\/[\w-]+\.(crt|key)|join\([^)]*'certs'\)/
    const skip = ['cgi-bin', 'node_modules', 'pki', 'tmp', 'web']
    for (const entry of readdirSync(TEST, { withFileTypes: true })) {
        const dir = entry.name
        if (!entry.isDirectory() || skip.includes(dir)) {
            continue
        }
        const path = join(TEST, dir)
        for (const name of readdirSync(path)) {
            if (name == self || name == 'certs-sync.tst.ts' || !/\.(c|h|ts|sh)$/.test(name)) {
                continue
            }
            const match = readFileSync(join(path, name), 'utf8').match(pattern)
            ttrue(match == null, `test/${dir}/${name} must read test/pki, not certs/ (found "${match?.[0]}")`)
        }
    }
}

function openssl(dir: string, ...args: string[]): string {
    const result = spawnSync(String(Cmd.locate('openssl')), args, { cwd: dir, encoding: 'utf8' })
    teq(result.status, 0, `openssl ${args.slice(0, 3).join(' ')}: ${result.stderr}`)
    return result.stdout
}

function checkGenerated() {
    const root = mkdtempSync(join(tmpdir(), 'goahead-test-pki-'))
    try {
        //  A tree with the generator and no certs/, as in a source distribution
        mkdirSync(join(root, 'test', 'utils'), { recursive: true })
        copyFileSync(join(TEST, 'utils', 'pki.sh'), join(root, 'test', 'utils', 'pki.sh'))
        const pki = join(root, 'test', 'pki')
        const result = spawnSync('bash', [join(root, 'test', 'utils', 'pki.sh'), pki], { encoding: 'utf8' })
        teq(result.status, 0, `pki.sh generates the PKI: ${result.stderr}`)

        const files = NAMES.flatMap(name => [`${name}.crt`, `${name}.key`]).sort()
        teq(readdirSync(pki).sort().join(' '), files.join(' '), 'the PKI holds each certificate and key, nothing else')

        for (const name of NAMES) {
            teq(openssl(pki, 'x509', '-in', `${name}.crt`, '-pubkey', '-noout'),
                openssl(pki, 'pkey', '-in', `${name}.key`, '-pubout'), `${name}.key is the key of ${name}.crt`)
        }
        tcontains(openssl(pki, 'verify', '-CAfile', 'ca.crt', 'test.crt'), 'test.crt: OK', 'ca issues test')
        tcontains(openssl(pki, 'x509', '-in', 'ca.crt', '-noout', '-text'), 'CA:TRUE', 'ca is a certificate authority')
        let text = openssl(pki, 'x509', '-in', 'test.crt', '-noout', '-text')
        tcontains(text, 'CA:FALSE', 'test is a leaf')
        tcontains(text, 'TLS Web Client Authentication', 'test may authenticate a client')
        tcontains(text, 'TLS Web Server Authentication', 'test may authenticate a server')
        for (const name of ['self', 'ec']) {
            let names = openssl(pki, 'x509', '-in', `${name}.crt`, '-noout', '-subject', '-issuer').split('\n')
            teq(names[0].replace(/^subject=/, ''), names[1].replace(/^issuer=/, ''), `${name} is self-signed`)
        }
        tcontains(openssl(pki, 'x509', '-in', 'ec.crt', '-noout', '-text'), 'id-ecPublicKey', 'ec has an EC key')
        tcontains(openssl(pki, 'x509', '-in', 'self.crt', '-noout', '-text'), 'rsaEncryption', 'self has an RSA key')
    } finally {
        rmSync(root, { recursive: true, force: true })
    }
}

checkSources()
if (String(Config.OS).startsWith('win') || !Cmd.locate('openssl')) {
    tskip('Requires bash and the openssl command')
} else {
    checkGenerated()
}
