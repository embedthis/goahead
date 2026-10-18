/* webcomp records each page's modified time and content hash */

import { ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync, utimesSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

const dir = mkdtempSync(join(tmpdir(), 'webcomp-'))
try {
    let one = join(dir, 'one.js')
    let two = join(dir, 'two.js')
    writeFileSync(one, 'version-1')
    writeFileSync(two, 'version-2')
    utimesSync(one, 1700000000, 1700000000)
    utimesSync(two, 1700000100, 1700000100)
    let list = join(dir, 'files.txt')
    writeFileSync(list, one + '\n' + two + '\n')

    let result = spawnSync('webcomp', ['--strip', dir, list], { encoding: 'utf8' })
    ttrue(result.status == 0, `webcomp must succeed, got ${result.status} ${result.stderr}`)

    let entry = (name: string) => {
        let match = result.stdout.match(new RegExp('\\{ "/' + name + '", p\\d+, (\\d+), (\\d+), (0x[0-9a-f]+)ULL \\}'))
        return match ? { size: match[1], mtime: match[2], hash: match[3] } : null
    }
    let a = entry('one.js')
    let b = entry('two.js')
    ttrue(a != null && b != null, `each page must be indexed with its time and hash:\n${result.stdout.slice(-400)}`)
    if (a && b) {
        ttrue(a.size == b.size, 'the two pages are the same size')
        ttrue(a.hash != b.hash, 'pages with different content must have different hashes')
        ttrue(a.mtime == '1700000000', 'the entry must carry the file modified time')
        ttrue(b.mtime == '1700000100', 'each entry carries its own file modified time')
    }
    ttrue(result.stdout.includes('{ 0, 0, 0, 0, 0 }'), 'the index is terminated with every field zero')
} finally {
    rmSync(dir, { recursive: true, force: true })
}
