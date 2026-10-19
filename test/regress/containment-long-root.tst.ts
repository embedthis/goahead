/* Document containment fails closed when the root cannot be resolved */
import { Config } from '@embedthis/ejscript'
import { tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const BASE = join(import.meta.dir, 'containment-long-root.dir')
const ENDPOINT = '127.0.0.1:18980'
const SEGMENT = 'd'.repeat(200)
//  Longer than PATH_MAX on macOS (1024) and Linux (4096)
const DEPTH = 22

//  rm cannot remove a tree deeper than PATH_MAX, so descend and remove one level at a time
function removeHome() {
    let script = `cd '${BASE}' 2>/dev/null || exit 0; depth=0; ` +
        `while [ -d ${SEGMENT}$depth ]; do cd ${SEGMENT}$depth; depth=$((depth + 1)); done; ` +
        `rm -rf web outside route.txt auth.txt; ` +
        `while [ $depth -gt 0 ]; do depth=$((depth - 1)); cd ..; rm -rf ${SEGMENT}$depth; done; ` +
        `cd .. && rmdir '${BASE}'`
    spawnSync('bash', ['-c', script])
}

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('POSIX containment only')
} else {
    let cdHome = Array.from({ length: DEPTH }, (_, i) => `cd ${SEGMENT}${i}`).join(' && ')
    let mkHome = Array.from({ length: DEPTH }, (_, i) => `mkdir ${SEGMENT}${i} && cd ${SEGMENT}${i}`).join(' && ')
    removeHome()
    let made = spawnSync('bash', ['-c', `mkdir -p '${BASE}' && cd '${BASE}' && ${mkHome} && mkdir web outside && ` +
        `echo INDEX > web/index.html && echo DEEP-SECRET > outside/secret.txt && ` +
        `ln -s ../outside/secret.txt web/lfile && printf 'route uri=/\\n' > route.txt && : > auth.txt && pwd | wc -c`],
        { encoding: 'utf8' })
    ttrue(made.status == 0, `cannot build the long home: ${made.stderr}`)
    ttrue(parseInt(made.stdout) > 4096, `the home path must exceed PATH_MAX, got ${made.stdout.trim()} bytes`)

    let log = ''
    let server = startServer('bash', ['-c', `cd '${BASE}' && ${cdHome} && exec goahead -v --route route.txt web ${ENDPOINT}`])
    server.stdout.on('data', (data) => log += data)
    server.stderr.on('data', (data) => log += data)
    try {
        let index: Response | null = null
        for (let i = 0; i < 50 && !index; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                index = await fetch(`http://${ENDPOINT}/index.html`)
            } catch {}
        }
        ttrue(index != null, `the server must start: ${log}`)
        let response = await fetch(`http://${ENDPOINT}/lfile`)
        let text = await response.text()
        ttrue(response.status == 404, `a symlink out of an unresolvable root must be refused, got ${response.status}`)
        ttrue(!text.includes('DEEP-SECRET'), 'the file outside the root must not be served')
    } finally {
        await stopServer(server)
        removeHome()
    }
}
