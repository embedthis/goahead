/* webcomp indexes each page with the size and hash of the bytes it emitted, whatever the file name holds */
import { tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'

if (process.platform == 'win32') {
    tskip('A backslash in a file name is a separator on Windows')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'webcomp-size-'))
    try {
        const big = join(dir, 'big.html')
        const odd = join(dir, 'x\\y.html')
        const quoted = join(dir, 'say"hi.html')
        writeFileSync(big, '0123456789')
        writeFileSync(odd, 'abc')
        writeFileSync(quoted, 'hello')
        const list = join(dir, 'files.txt')
        writeFileSync(list, big + '\n' + odd + '\n' + quoted + '\n')
        const result = spawnSync('webcomp', ['--strip', dir, list], { encoding: 'utf8' })
        ttrue(result.status == 0, `webcomp must succeed, got ${result.status} ${result.stderr}`)

        const entries = [...result.stdout.matchAll(/\{ "([^"\\]*(?:\\.[^"\\]*)*)", p(\d+), (\d+), (\d+), (0x[0-9a-f]+)ULL \}/g)]
            .map(m => ({ path: m[1], page: m[2], size: parseInt(m[3]), hash: m[5] }))
        ttrue(entries.length == 3, `three pages are indexed, got ${entries.length}:\n${result.stdout.slice(-600)}`)
        const bySize: Record<string, number> = { '/big.html': 10, '/x/y.html': 3, '/say\\"hi.html': 5 }
        for (const entry of entries) {
            ttrue(entry.path in bySize, `an indexed path is one of the inputs: ${entry.path}`)
            ttrue(entry.size == bySize[entry.path], `${entry.path} is indexed at ${bySize[entry.path]} bytes, got ${entry.size}`)
            ttrue(entry.hash != '0xcbf29ce484222325', `${entry.path} carries a hash of its content`)
            //  The page array holds exactly size bytes plus the terminating zero
            const array = result.stdout.match(new RegExp('static uchar p' + entry.page + '\\[\\] = \\{([^}]*)\\}'))
            const count = array ? array[1].split(',').map(s => s.trim()).filter(s => s.length).length : -1
            ttrue(count == entry.size + 1, `${entry.path}'s array holds ${entry.size} bytes and a terminator, got ${count}`)
        }
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
