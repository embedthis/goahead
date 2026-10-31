/*
    eval-package.tst - An evaluation snapshot ships no CRA documents or SBOM, says it is not a release, writes no
    release record and cannot be signed
 */
import { tcontains, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const BUILD_LIB = join(ROOT, 'bin', 'buildLib.sh')

function readOrEmpty(path: string): string {
    return existsSync(path) ? readFileSync(path, 'utf8') : ''
}

if (process.platform == 'win32') {
    tskip('The packaging script is bash')
} else if (!existsSync(BUILD_LIB)) {
    tskip('The packaging script is not in a source distribution')
} else {
    let dir = mkdtempSync(join(tmpdir(), 'goahead-eval-'))
    try {
        let version = JSON.parse(readFileSync(join(ROOT, 'package.json'), 'utf8')).version
        let record = join(ROOT, 'doc', 'releases', version, 'SHA256SUMS')
        let recordBefore = readOrEmpty(record)
        let env = { ...process.env, EVAL_BUILD: '1', PKG_BUILD: join(dir, 'build'), DIST: join(dir, 'dist') }
        delete env.MINISIGN_KEY

        let result = spawnSync('bash', [BUILD_LIB], { encoding: 'utf8', env })
        ttrue(result.status == 0, `evaluation packaging must succeed: ${result.stderr}`)
        ttrue(!/missing from/.test(result.stderr), 'evaluation packaging must not warn about missing CRA documents')
        ttrue(!/Not signed/.test(result.stdout + result.stderr), 'evaluation packaging must not mention signing')

        let listing = spawnSync('tar', ['-tzf', join(dir, 'build', 'goahead-src.tgz')], { encoding: 'utf8' }).stdout
        for (let name of ['declaration-of-conformity.md', 'user-information.md', 'sbom.json']) {
            ttrue(!listing.includes('/doc/' + name), `an evaluation snapshot must not ship doc/${name}`)
        }
        ttrue(listing.includes(`goahead-${version}/EVALUATION.md`), 'an evaluation snapshot must ship EVALUATION.md')
        let notice = spawnSync('tar', ['-xzOf', join(dir, 'build', 'goahead-src.tgz'),
            `goahead-${version}/EVALUATION.md`], { encoding: 'utf8' }).stdout
        tcontains(notice, 'It is not a release.', 'the notice must say the snapshot is not a release')
        tcontains(notice, 'no EU Declaration of Conformity', 'the notice must say there is no conformity')

        ttrue(readOrEmpty(record) == recordBefore, 'an evaluation snapshot must not write the release record')

        result = spawnSync('bash', [BUILD_LIB], { encoding: 'utf8', env: { ...env, MINISIGN_KEY: '/keys/release.key' } })
        ttrue(result.status != 0, 'an evaluation snapshot must refuse to be signed')
        tcontains(result.stderr, 'cannot be signed', 'and say why')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
