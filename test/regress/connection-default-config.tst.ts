/* Compile real header configurations after system headers have defined FD_SETSIZE. */
import { Cmd, Config } from '@embedthis/ejscript'
import { teq, ttrue } from '@embedthis/testme'
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'
import { spawnSync } from 'node:child_process'

const root = join(import.meta.dir, '..', '..')
const windows = String(Config.OS).startsWith('win')
const cc = windows ? Cmd.locate('cl') : (Cmd.locate('cc') || Cmd.locate('gcc'))
ttrue(!!cc, 'C compiler available')
const dir = mkdtempSync(join(tmpdir(), 'goahead-connection-config-'))
try {
    function compile(size: number, expected: number, override?: number) {
        const path = join(dir, 'probe.c')
        writeFileSync(path, '#include "osdep.h"\n#undef ME_MAX_IP\n#undef FD_SETSIZE\n#define FD_SETSIZE ' + size + '\n' +
            (override === undefined ? '' : '#define ME_GOAHEAD_LIMIT_CONNECTIONS ' + override + '\n') +
            '#include "goahead.h"\n' +
            'typedef char check_limit[(ME_GOAHEAD_LIMIT_CONNECTIONS == ' + expected + ') ? 1 : -1];\n')
        const flags = windows
            ? ['/nologo', '/c', '/I' + join(root, 'src'), '/I' + join(root, 'src/osdep'), '/Fo' + join(dir, 'probe.obj')]
            : ['-fsyntax-only', '-Werror', '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep')]
        return spawnSync(String(cc), [...flags, path], { encoding: 'utf8' })
    }
    for (const size of [8, 16, 32, 33, 40, 41, 64, 1024]) {
        const result = compile(size, size > 40 ? size - 32 : 8)
        teq(result.status, 0, 'usable default for FD_SETSIZE=' + size + ': ' + result.stderr)
    }
    teq(compile(16, 3, 3).status, 0, 'positive explicit limit preserved')
    for (const limit of [0, -1]) {
        const result = compile(16, limit, limit)
        ttrue(result.status !== 0, 'non-positive explicit limit rejected')
        ttrue((result.stdout + result.stderr).includes('ME_GOAHEAD_LIMIT_CONNECTIONS must be positive'),
            'invalid limit has a clear diagnostic')
    }
} finally {
    rmSync(dir, { recursive: true, force: true })
}
