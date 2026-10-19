/*
    server-orphan.tst - A server a test starts never outlives the test, and one that does fails the run

    A failed assertion, an exception and a signal each stop the test's server and remove its record. A test killed
    outright leaves its record, and test/utils/reap-servers.sh then stops the server and names the test, which fails
    test/utils/check-server.sh. Every test that starts a server must use utils/server.ts, or none of this applies.
 */
import { tcontains, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { createServer } from 'node:net'
import { tmpdir } from 'node:os'
import { join, relative } from 'node:path'
import { atExit } from '../utils/server'

const TESTDIR = join(import.meta.dir, '..')
const CHILD = join(import.meta.dir, 'server-orphan-child.ts')
const REAP = join(TESTDIR, 'utils', 'reap-servers.sh')
const CHECK = join(TESTDIR, 'utils', 'check-server.sh')
const FIXTURES = join(import.meta.dir, 'server-shutdown-gate')

function alive(pid: number): boolean {
    try {
        process.kill(pid, 0)
        return true
    } catch {
        return false
    }
}

async function gone(pid: number): Promise<boolean> {
    for (let i = 0; i < 50 && alive(pid); i++) {
        await new Promise(r => setTimeout(r, 100))
    }
    return !alive(pid)
}

async function freePort(): Promise<number> {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    return port
}

async function start(mode: string, servers: string) {
    const child = Bun.spawn(['bun', CHILD, mode, String(await freePort())],
        { stdout: 'pipe', stderr: 'pipe', env: { ...process.env, GOAHEAD_TEST_SERVERS: servers } })
    const reader = child.stdout.getReader()
    let out = ''
    while (!out.includes('\n')) {
        const { done, value } = await reader.read()
        if (done) break
        out += new TextDecoder().decode(value)
    }
    reader.releaseLock()
    const pid = parseInt((out.match(/ready (\d+)/) || [])[1] || '0')
    return { child, pid, recorded: out.includes(' recorded') }
}

//  Every test that starts a server asynchronously must do it through utils/server.ts
function unguarded(dir: string, found: string[] = []): string[] {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
        const path = join(dir, entry.name)
        if (entry.isDirectory()) {
            if (!['sec', 'node_modules', 'utils'].includes(entry.name)) unguarded(path, found)
        } else if (entry.name.endsWith('.tst.ts')) {
            const source = readFileSync(path, 'utf8')
            const imports = [...source.matchAll(/import\s*\{([^}]*)\}\s*from\s*['"]node:child_process['"]/g)]
                .flatMap(m => m[1].split(',').map(name => name.trim()))
            if (imports.includes('spawn') || /Bun\.spawn\(\s*\[[^\]]*goahead/.test(source)) {
                found.push(relative(TESTDIR, path))
            }
        }
    }
    return found
}

if (process.platform == 'win32') {
    tskip('Process groups and test/utils/reap-servers.sh are POSIX')
} else {
    let offenders = unguarded(TESTDIR)
    ttrue(offenders.length == 0, 'tests must start servers with startServer() from utils/server.ts, not spawn(): ' +
        offenders.join(', '))

    const servers = mkdtempSync(join(tmpdir(), 'goahead-servers-'))
    const orphans = servers + '-orphans.txt'
    atExit(() => {
        spawnSync('bash', [REAP, servers])
        rmSync(servers, { recursive: true, force: true })
        rmSync(orphans, { force: true })
    })
    for (const mode of ['assert', 'throw', 'wait']) {
        const { child, pid, recorded } = await start(mode, servers)
        ttrue(pid > 0, `${mode}: the child's server must start`)
        ttrue(recorded, `${mode}: the running server must be recorded`)
        if (mode == 'wait') {
            child.kill('SIGTERM')
        }
        const status = await child.exited
        ttrue(status != 0, `${mode}: the child must fail, got ${status}`)
        ttrue(await gone(pid), `${mode}: the server must not outlive its test`)
        ttrue(!existsSync(join(servers, String(pid))), `${mode}: the server's record must be removed`)
    }

    //  A test killed outright cannot stop its server: the record survives it and the reaper stops the server
    const { child, pid } = await start('wait', servers)
    ttrue(pid > 0, 'kill: the child\'s server must start')
    child.kill('SIGKILL')
    await child.exited
    ttrue(alive(pid), 'kill: a SIGKILLed test leaves its server running')
    const reaped = spawnSync('bash', [REAP, servers], { encoding: 'utf8' })
    ttrue(reaped.status == 1, `the reaper must report the left server, got status ${reaped.status}`)
    tcontains(reaped.stdout, 'regress/server-orphan-child.ts left a server running', 'and name the test')
    ttrue(await gone(pid), 'the reaper must stop the server')
    ttrue(readdirSync(servers).length == 0, 'the reaper must remove the record')

    writeFileSync(orphans, reaped.stdout)
    const failed = spawnSync('bash', [CHECK, join(FIXTURES, 'clean-log.txt'), join(FIXTURES, 'exit-0.txt'), orphans],
        { encoding: 'utf8' })
    ttrue(failed.status != 0, 'check-server.sh must fail the run when a test left a server running')
    tcontains(failed.stderr, 'regress/server-orphan-child.ts left a server running', 'and name the test')

    writeFileSync(orphans, '')
    const passed = spawnSync('bash', [CHECK, join(FIXTURES, 'clean-log.txt'), join(FIXTURES, 'exit-0.txt'), orphans],
        { encoding: 'utf8' })
    ttrue(passed.status == 0, 'check-server.sh must pass when no test left a server: ' + passed.stderr)
}
