/*
    tls-client-cert-build-clean.tst - The client certificate configurations compile without warnings

    The release build and every CI feature leg fail on a compiler warning, so a configuration the deployment guide
    recommends must compile clean under the shipped warning set. A revocation list (ME_GOAHEAD_SSL_REVOKE) is the
    one TLS setting no CI leg builds.
 */
import { Cmd, Config } from '@embedthis/ejscript'
import { teq, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { join } from 'node:path'

const cc = Cmd.locate('cc')

/*
    The POSIX warning flags from projects/premake5.lua, plus the clang-only additions when the compiler is clang.
 */
const WARNINGS = ['-Wall', '-Wextra', '-Wno-unused-parameter', '-Wshadow', '-Wformat=2', '-Wformat-security',
    '-Wsign-compare', '-Werror=format', '-Werror=implicit-function-declaration', '-Werror=return-type',
    '-Werror=int-conversion', '-Werror=null-dereference']

const CASES = [
    { name: 'revocation list', defines: ['-DME_GOAHEAD_SSL_REVOKE="crl.pem"'] },
    { name: 'authority', defines: ['-DME_GOAHEAD_SSL_AUTHORITY="ca.crt"'] },
    {
        name: 'client certificates with an authority and a revocation list',
        defines: ['-DME_GOAHEAD_SSL_VERIFY_PEER=1', '-DME_GOAHEAD_SSL_VERIFY_ISSUER=1',
            '-DME_GOAHEAD_SSL_AUTHORITY="ca.crt"', '-DME_GOAHEAD_SSL_REVOKE="crl.pem"'],
    },
    {
        name: 'client certificates verified by key only',
        defines: ['-DME_GOAHEAD_SSL_VERIFY_PEER=1', '-DME_GOAHEAD_SSL_AUTHORITY="ca.crt"', '-DME_GOAHEAD_SSL_REVOKE="crl.pem"'],
    },
    { name: 'default', defines: [] },
]

if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX compiler')
} else {
    const src = join(import.meta.dir, '..', '..', 'src')
    const version = spawnSync(String(cc), ['--version'], { encoding: 'utf8' }).stdout || ''
    const clang = /clang/.test(version) ? ['-Wshorten-64-to-32', '-Wimplicit-int-conversion'] : []

    /*
        http.c is the only unit that tests the TLS string settings against each other, and it needs no TLS headers.
     */
    function check(defines: string[]) {
        return spawnSync(String(cc), ['-fsyntax-only', ...WARNINGS, ...clang, '-std=gnu11', '-I' + src,
            '-I' + join(src, 'osdep'), '-DME_VERSION="test"', '-DME_COM_SSL=1', '-DME_COM_OPENSSL=1',
            '-DME_COM_MBEDTLS=0', ...defines, join(src, 'http.c')], { encoding: 'utf8' })
    }

    for (let { name, defines } of CASES) {
        let result = check(defines)
        teq(result.status, 0, `${name} must compile: ${result.stderr}`)
        let warnings = result.stderr.split('\n').filter(line => /warning:/.test(line))
        teq(warnings.length, 0, `${name} must compile without warnings:\n${warnings.join('\n')}`)
    }
}
