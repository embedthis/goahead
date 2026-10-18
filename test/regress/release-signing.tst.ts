/*
    release-signing.tst - make package signs the release archive when given a signing key
 */
import { tcontains, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { chmodSync, existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const SIGN = join(ROOT, 'bin', 'sign.sh')

if (process.platform == 'win32') {
    tskip('The release scripts are bash')
} else if (!existsSync(join(ROOT, 'bin', 'buildLib.sh'))) {
    tskip('The release scripts are not in a source distribution')
} else {
    let dir = mkdtempSync(join(tmpdir(), 'goahead-sign-'))
    try {
        let archive = join(dir, 'goahead-1.2.3-src.tgz')
        writeFileSync(archive, 'archive bytes')
        let stubs = join(dir, 'stubs')
        spawnSync('mkdir', [stubs])
        let stub = join(stubs, 'minisign')
        writeFileSync(stub, '#!/bin/bash\necho "$@" > "' + join(dir, 'args') + '"\n' +
                            'while [ $# -gt 0 ] ; do [ "$1" = "-m" ] && echo sig > "$2.minisig" ; shift ; done\n')
        chmodSync(stub, 0o755)

        let run = (env: Record<string, string>) => spawnSync('bash', [SIGN, archive],
            { encoding: 'utf8', env: { HOME: dir, ...env } })

        let result = run({ PATH: stubs + ':/usr/bin:/bin' })
        ttrue(result.status == 0, 'without MINISIGN_KEY packaging must succeed')
        ttrue(!existsSync(archive + '.minisig'), 'and nothing is signed')

        result = run({ PATH: stubs + ':/usr/bin:/bin', MINISIGN_KEY: '/keys/release.key' })
        ttrue(result.status == 0, 'with MINISIGN_KEY signing must succeed: ' + result.stderr)
        ttrue(existsSync(archive + '.minisig'), 'and write the signature beside the archive')
        let args = readFileSync(join(dir, 'args'), 'utf8')
        tcontains(args, '-S -s /keys/release.key -m ' + archive, 'the archive must be signed with the named key')

        rmSync(archive + '.minisig')
        result = run({ PATH: '/usr/bin:/bin', MINISIGN_KEY: '/keys/release.key' })
        ttrue(result.status != 0, 'with MINISIGN_KEY but no minisign, signing must fail')
        ttrue(!existsSync(archive + '.minisig'), 'and write no signature')

        let build = readFileSync(join(ROOT, 'bin', 'buildLib.sh'), 'utf8')
        tcontains(build, '"$TOP/bin/sign.sh" "$BUILD/${NAME}-src.tgz"', 'the package build must sign the release archive')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
