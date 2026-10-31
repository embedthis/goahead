/*
    replace-malloc-release-warning.tst - The replacement allocator compiles warning-free when assert is compiled out.

    wvalid (src/alloc.c) checked the block only through assert, which ME_GOAHEAD_DEBUG=0 (every release build)
    and ME_GOAHEAD_LOGGING=0 define away, leaving its only variable set but unused. CI fails a feature leg or a
    release build on any warning, but builds the replacement allocator only as a logging debug build, so the
    warning reached integrators unseen. The release allocator build must compile clean and CI must build it.
 */

import { Cmd } from '@embedthis/ejscript'
import { existsSync, readFileSync } from 'node:fs'
import { tskip, ttrue } from '@embedthis/testme'
import { join } from 'node:path'
import { cc, isWindows, ROOT } from '../utils/amalgamation'

const WORKFLOW = join(ROOT, '.github', 'workflows', 'ci.yml')

if (isWindows) {
    tskip('Compiling src/alloc.c directly is a POSIX-only arm of this test')
} else if (!cc) {
    tskip('No C compiler on PATH to compile src/alloc.c with the replacement allocator')
} else {
    function compile(label: string, defines: string): {ok: boolean, output: string} {
        let cmd = new Cmd(cc + ' -c -o /dev/null -Wall -Wextra -Wno-unused-parameter -Werror -std=gnu11' +
            ' -I' + join(ROOT, 'src') + ' -I' + join(ROOT, 'src', 'osdep') +
            ' -DME_COM_SSL=0 -DME_COM_OPENSSL=0 -DME_COM_MBEDTLS=0 -DME_COMPILER_HAS_PAM=0' +
            ' -DME_VERSION=\\"test\\" -DME_GOAHEAD_REPLACE_MALLOC=1 ' + defines + ' ' + join(ROOT, 'src', 'alloc.c'))
        return {ok: cmd.status == 0, output: String(cmd.error) + String(cmd.response)}
    }

    /* Control: the debug allocator build that CI already compiles */
    let debug = compile('debug', '-DME_DEBUG=1')
    ttrue(debug.ok, 'The replacement allocator must compile warning-free in a debug build:\n' + debug.output)

    /* Each build that defines assert away must be as clean */
    for (let [label, defines] of [['release', '-DME_DEBUG=0'], ['logging-off', '-DME_DEBUG=1 -DME_GOAHEAD_LOGGING=0'],
                                  ['no-asserts', '-DME_DEBUG=1 -DME_GOAHEAD_DEBUG=0']]) {
        let r = compile(label, defines)
        ttrue(r.ok, 'The replacement allocator must compile warning-free in a ' + label + ' build (' + defines +
            '):\n' + r.output)
    }
}

/* CI must build the release allocator configuration, where the warning gate applies */
if (!existsSync(WORKFLOW)) {
    tskip('The CI workflows are not part of a source distribution')
} else {
    let ci = Bun.YAML.parse(readFileSync(WORKFLOW, 'utf8')) as any
    let legs: any[] = ci.jobs?.features?.strategy?.matrix?.include || []
    let leg = legs.find(l => l.leg == 'release-replace-malloc')
    ttrue(leg != null, 'the features job must have a release-replace-malloc leg')
    if (leg) {
        ttrue(leg.optimize == 'release', 'release-replace-malloc must build release')
        ttrue(`${leg.cflags || ''}`.split(/\s+/).includes('-DME_GOAHEAD_REPLACE_MALLOC=1'),
            'release-replace-malloc must build with -DME_GOAHEAD_REPLACE_MALLOC=1')
    }
}
