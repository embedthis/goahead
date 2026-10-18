/*
    mbedtls-crl-coverage.tst.ts - With a CRL file set, a client chain through a CA that has no CRL in the file is
    refused, as it is with OpenSSL
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const cc = Cmd.locate('cc')
//  The fixture links the built library, so it compiles with the library's feature flags
const FEATURES = String(tget('TM_FEATURES_HEADER'))
const ssl = Cmd.locate('openssl')
const pkg = spawnSync('pkg-config', ['--cflags', '--libs', 'mbedtls', 'mbedx509', 'mbedcrypto'], { encoding: 'utf8' })

if (String(Config.OS).startsWith('win') || !cc || !ssl || pkg.status !== 0) {
    tskip('Requires a POSIX compiler, the openssl command and MbedTLS development packages via pkg-config')
} else {
    const root = join(import.meta.dir, '..', '..')
    const certs = join(root, 'test', 'pki')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-mbed-crl-'))
    const fixture = join(import.meta.dir, 'mbedtls-crl-coverage-fixture.c')

    function openssl(...args: string[]) {
        const result = spawnSync(String(ssl), args, { cwd: dir, encoding: 'utf8' })
        teq(result.status, 0, `openssl ${args.slice(0, 2).join(' ')}: ${result.stderr}`)
    }

    function issue(name: string, subject: string, issuer: string | null, ext: string) {
        openssl('genpkey', '-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:P-256', '-out', `${name}.key`)
        openssl('req', '-new', '-key', `${name}.key`, '-subj', subject, '-out', `${name}.csr`)
        if (issuer) {
            openssl('x509', '-req', '-in', `${name}.csr`, '-CA', `${issuer}.crt`, '-CAkey', `${issuer}.key`,
                '-CAcreateserial', '-days', '30', '-extfile', 'ext.cnf', '-extensions', ext, '-out', `${name}.crt`)
        } else {
            openssl('x509', '-req', '-in', `${name}.csr`, '-signkey', `${name}.key`, '-days', '30',
                '-extfile', 'ext.cnf', '-extensions', ext, '-out', `${name}.crt`)
        }
    }

    //  A CRL issued by ca, revoking the certificates named in revoked
    function crl(ca: string, out: string, revoked: string[] = []) {
        writeFileSync(join(dir, `${ca}-${out}.index`), '')
        writeFileSync(join(dir, `${ca}-${out}.num`), '1000\n')
        writeFileSync(join(dir, `${ca}-${out}.cnf`),
            `[ca]\ndefault_ca=x\n[x]\ndatabase=${ca}-${out}.index\ncrlnumber=${ca}-${out}.num\n` +
            'default_md=sha256\ndefault_crl_days=30\n')
        for (let name of revoked) {
            openssl('ca', '-config', `${ca}-${out}.cnf`, '-keyfile', `${ca}.key`, '-cert', `${ca}.crt`,
                '-revoke', `${name}.crt`)
        }
        openssl('ca', '-config', `${ca}-${out}.cnf`, '-keyfile', `${ca}.key`, '-cert', `${ca}.crt`, '-gencrl',
            '-out', `${out}.pem`)
    }

    function join2(out: string, ...files: string[]) {
        writeFileSync(join(dir, out), files.map(f => readFileSync(join(dir, f), 'utf8')).join(''))
    }

    function attempt(name: string, crls: string) {
        const exe = join(dir, name)
        const args = ['-include', FEATURES,
            '-DME_COM_MBEDTLS=1', '-DME_COM_OPENSSL=0', '-DME_GOAHEAD_SSL_VERIFY_PEER=1',
            '-DME_GOAHEAD_SSL_VERIFY_ISSUER=1', `-DME_GOAHEAD_SSL_AUTHORITY="${join(dir, 'root.crt')}"`,
            `-DME_GOAHEAD_SSL_REVOKE="${join(dir, crls)}"`, '-Wformat', '-Werror=format',
            '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep'), fixture,
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead',
            ...pkg.stdout.trim().split(/\s+/), '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, `the fixture compiles for ${name}: ${built.stderr}`)
        const result = spawnSync(exe, [certs, join(dir, 'client-chain.pem'), join(dir, 'client.key')],
            { encoding: 'utf8', timeout: 60000 })
        teq(result.status, 0, `the fixture runs for ${name}: ${result.stderr}`)
        return result.stdout
    }

    try {
        mkdirSync(dir, { recursive: true })
        writeFileSync(join(dir, 'ext.cnf'),
            '[ca]\nbasicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n' +
            'subjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid\n' +
            '[leaf]\nbasicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=clientAuth\n')
        issue('root', '/CN=CRL Root', null, 'ca')
        issue('inter', '/CN=CRL Intermediate', 'root', 'ca')
        issue('client', '/CN=CRL Client', 'inter', 'leaf')
        join2('client-chain.pem', 'client.crt', 'inter.crt')

        crl('root', 'root-crl')
        crl('inter', 'inter-crl')
        crl('inter', 'inter-revokes-client-crl', ['client'])
        join2('root-only.pem', 'root-crl.pem')
        join2('both.pem', 'root-crl.pem', 'inter-crl.pem')
        join2('revoked.pem', 'root-crl.pem', 'inter-revokes-client-crl.pem')

        //  The client's issuer has no CRL in the file, so the client's revocation status is unknown
        tcontains(attempt('missing', 'root-only.pem'), 'client chain refused',
            'a chain through a CA with no CRL in the file is refused')
        tcontains(attempt('covered', 'both.pem'), 'client chain accepted',
            'a chain whose CAs all have CRLs is accepted')
        tcontains(attempt('revoked', 'revoked.pem'), 'client chain refused', 'a revoked client is refused')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
