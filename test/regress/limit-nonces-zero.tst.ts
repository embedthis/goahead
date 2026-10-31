/*
    limit-nonces-zero.tst - A build whose Digest nonce table may hold no entries must not compile.

    evictNonces (src/auth.c) bounds the nonce table with "while (nonceTotal >= ME_GOAHEAD_LIMIT_NONCES)".
    With the limit at 0 the condition never clears, so the first successful Digest authentication
    (websAcceptDigestCount) spins the single-threaded server forever and no later request is served.
    Six sibling limits document 0 as "disabled", so the setting must be refused at compile time.
 */

import { Cmd } from '@embedthis/ejscript'
import { tskip, ttrue } from '@embedthis/testme'
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'
import { cc, isWindows, ROOT } from '../utils/amalgamation'

/*
    Only the configuration header is compiled: the guard is a preprocessor check, so no library is needed.
 */
const DRIVER = `
#include "goahead.h"

int main(int argc, char **argv)
{
    return 0;
}
`

if (isWindows) {
    tskip('Rebuilding against src/goahead.h is a POSIX-only arm of this test')
} else if (!cc) {
    tskip('No C compiler on PATH to compile goahead.h at ME_GOAHEAD_LIMIT_NONCES=0')
} else {
    let dir = mkdtempSync(join(tmpdir(), 'goahead-nonces-'))
    writeFileSync(join(dir, 'driver.c'), DRIVER)

    function compile(label: string, defines: string): {ok: boolean, output: string} {
        let cmd = new Cmd(cc + ' -fsyntax-only -I' + join(ROOT, 'src') + ' -I' + join(ROOT, 'src', 'osdep') +
            ' -DME_COM_SSL=0 -DME_COM_OPENSSL=0 -DME_COM_MBEDTLS=0 -DME_COMPILER_HAS_PAM=0' +
            ' -DME_VERSION=\\"test\\" ' + defines + ' ' + join(dir, 'driver.c'))
        return {ok: cmd.status == 0, output: String(cmd.error) + String(cmd.response)}
    }

    /* Control: the default limit compiles */
    let dflt = compile('default', '')
    ttrue(dflt.ok, 'goahead.h must compile at the default ME_GOAHEAD_LIMIT_NONCES:\n' + dflt.output)

    /* A limit of zero must be refused and the refusal must name the setting */
    let zero = compile('nonces0', '-DME_GOAHEAD_LIMIT_NONCES=0')
    ttrue(!zero.ok, 'ME_GOAHEAD_LIMIT_NONCES=0 must not compile: evictNonces never terminates and the first ' +
        'successful Digest authentication hangs the server')
    if (!zero.ok) {
        ttrue(/ME_GOAHEAD_LIMIT_NONCES/.test(zero.output), 'The compile error must name ME_GOAHEAD_LIMIT_NONCES:\n' +
            zero.output)
    }

    /* A negative limit is the same contract violation */
    let negative = compile('nonces-1', '-DME_GOAHEAD_LIMIT_NONCES=-1')
    ttrue(!negative.ok, 'A negative ME_GOAHEAD_LIMIT_NONCES must not compile')

    rmSync(dir, {recursive: true, force: true})
}
