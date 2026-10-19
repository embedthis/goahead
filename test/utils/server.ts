/*
    Start private test servers in process groups and stop them on exit or signal.
    Assertions call process.exit(), skipping finally blocks, so cleanup uses exit handlers.
    Record each server under SERVERS/PID so reap-servers.sh can clean up after SIGKILL.
 */
import { spawn, ChildProcess, SpawnOptions } from 'node:child_process'
import { mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { constants } from 'node:os'
import { join, relative } from 'node:path'

const TESTDIR = join(import.meta.dir, '..')
export const SERVERS = process.env.GOAHEAD_TEST_SERVERS || join(TESTDIR, '.servers')
const POSIX = process.platform != 'win32'

const servers = new Set<ChildProcess>()
const exits: (() => void)[] = []

function signal(server: ChildProcess, sig: NodeJS.Signals) {
    try {
        if (POSIX) {
            process.kill(-server.pid!, sig)
        } else {
            server.kill(sig)
        }
    } catch {}
}

function forget(server: ChildProcess) {
    servers.delete(server)
    rmSync(join(SERVERS, String(server.pid)), { force: true })
}

process.on('exit', () => {
    for (let server of servers) {
        signal(server, 'SIGKILL')
        forget(server)
    }
    for (let fn of exits) {
        try { fn() } catch {}
    }
})

for (let sig of ['SIGINT', 'SIGTERM', 'SIGHUP'] as NodeJS.Signals[]) {
    process.on(sig, () => process.exit(128 + constants.signals[sig]))
}

/*
    Register exit cleanup, including for daemons detached from the test.
 */
export function atExit(fn: () => void) {
    exits.push(fn)
}

export function startServer(command: string, args: string[], options: SpawnOptions = {}): ChildProcess {
    let server = spawn(command, args, { ...options, detached: POSIX })
    if (server.pid) {
        servers.add(server)
        mkdirSync(SERVERS, { recursive: true })
        writeFileSync(join(SERVERS, String(server.pid)), relative(TESTDIR, process.argv[1] || 'unknown') + '\n')
        server.once('exit', () => forget(server))
    }
    return server
}

export async function stopServer(server: ChildProcess, sig: NodeJS.Signals = 'SIGTERM') {
    if (server.exitCode == null && server.signalCode == null && servers.has(server)) {
        let exited = new Promise(resolve => server.once('exit', resolve))
        signal(server, sig)
        await exited
    }
}
