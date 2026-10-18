/*
    release-docs.tst - The release documents ship without HTML comments, and a release refuses an unsigned
    declaration or an unresolved placeholder
 */
import { tcontains, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const STAGE_DOCS = join(ROOT, 'bin', 'stageDocs.sh')

function stage(reldoc: string, dest: string, env: Record<string, string> = {}) {
    return spawnSync('bash', [STAGE_DOCS, reldoc, dest],
        { encoding: 'utf8', env: { PATH: '/usr/bin:/bin', ...env } })
}

function noComments(dir: string) {
    for (let name of readdirSync(dir)) {
        let text = readFileSync(join(dir, name), 'utf8')
        ttrue(!text.includes('<!--') && !text.includes('-->'), `${name} must ship without HTML comments`)
    }
}

if (process.platform == 'win32') {
    tskip('The release scripts are bash')
} else if (!existsSync(STAGE_DOCS)) {
    tskip('The release scripts are not in a source distribution')
} else {
    let dir = mkdtempSync(join(tmpdir(), 'goahead-docs-'))
    try {
        //  The documents of the current release
        let version = JSON.parse(readFileSync(join(ROOT, 'package.json'), 'utf8')).version
        let reldoc = join(ROOT, 'doc', 'releases', version)
        if (existsSync(reldoc)) {
            let dest = join(dir, 'current')
            let result = stage(reldoc, dest)
            ttrue(result.status == 0, `staging the ${version} documents must succeed: ${result.stderr}`)
            noComments(dest)
            if (existsSync(join(dest, 'declaration-of-conformity.md'))) {
                tcontains(readFileSync(join(dest, 'declaration-of-conformity.md'), 'utf8'),
                    '# EU Declaration of Conformity', 'and keep the declaration itself')
            }
        }

        //  Every comment form is stripped and the text around it kept
        let fixture = join(dir, 'fixture')
        mkdirSync(fixture)
        let issued = '# EU Declaration of Conformity\n\nIssued 1 January 2027.\n\n' +
                     '<!--\n    Internal guidance naming ~/internal/path. [TODO: internal]\n-->\n\n## 1. Product\n'
        writeFileSync(join(fixture, 'declaration-of-conformity.md'), issued)
        writeFileSync(join(fixture, 'user-information.md'),
            '# User Information\n\n<!-- REVIEW: internal note -->\nKeep this <!-- inline --> line.\n')
        writeFileSync(join(fixture, 'release-notes.md'), '# Notes\n<!--\n  embargo\n-->\n\nBody\n')

        let dest = join(dir, 'fixture-out')
        let result = stage(fixture, dest, { MINISIGN_KEY: '/keys/release.key' })
        ttrue(result.status == 0, `an issued declaration must stage for a release: ${result.stderr}`)
        noComments(dest)
        let decl = readFileSync(join(dest, 'declaration-of-conformity.md'), 'utf8')
        ttrue(!decl.includes('~/internal/path'), 'an internal path in a comment must not ship')
        tcontains(decl, 'Issued 1 January 2027.\n\n## 1. Product', 'the text around a comment must be kept')
        let info = readFileSync(join(dest, 'user-information.md'), 'utf8')
        ttrue(!info.includes('REVIEW'), 'a single-line comment must be stripped')
        tcontains(info, 'Keep this  line.', 'an inline comment must be stripped and its line kept')
        tcontains(readFileSync(join(dest, 'release-notes.md'), 'utf8'), 'Body', 'the release notes must keep their body')

        //  A release refuses an unsigned declaration, but development packaging stages the draft
        writeFileSync(join(fixture, 'declaration-of-conformity.md'),
            '# EU Declaration of Conformity\n\n> **STATUS: NOT ISSUED.**\n')
        result = stage(fixture, join(dir, 'unsigned'), { MINISIGN_KEY: '/keys/release.key' })
        ttrue(result.status != 0, 'a release must refuse an unsigned declaration')
        result = stage(fixture, join(dir, 'unsigned-dev'))
        ttrue(result.status == 0, 'development packaging must stage an unsigned declaration')

        //  A release refuses an unresolved placeholder outside a comment
        writeFileSync(join(fixture, 'declaration-of-conformity.md'), issued + '\n| Date | [FILL: date] |\n')
        result = stage(fixture, join(dir, 'fill'), { MINISIGN_KEY: '/keys/release.key' })
        ttrue(result.status != 0, 'a release must refuse an unresolved placeholder')

        //  A release refuses a missing declaration
        rmSync(join(fixture, 'declaration-of-conformity.md'))
        result = stage(fixture, join(dir, 'missing'), { MINISIGN_KEY: '/keys/release.key' })
        ttrue(result.status != 0, 'a release must refuse a missing declaration')
        writeFileSync(join(fixture, 'declaration-of-conformity.md'), issued)

        //  An unterminated comment cannot leak what follows it
        writeFileSync(join(fixture, 'user-information.md'), '# User Information\n<!-- unterminated\nsecret\n')
        result = stage(fixture, join(dir, 'unterminated'))
        ttrue(result.status != 0, 'an unterminated comment must fail staging')

        let build = readFileSync(join(ROOT, 'bin', 'buildLib.sh'), 'utf8')
        tcontains(build, '"$TOP/bin/stageDocs.sh" "$RELDOC" "$STAGE/doc"', 'the package build must stage the documents')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
