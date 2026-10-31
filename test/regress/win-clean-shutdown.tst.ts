/*
    win-clean-shutdown.tst - On Windows a close request stops the server through its shutdown path

    Invariant: goahead dispatches its window messages and installs a console control handler, so a WM_CLOSE to its
    taskbar window (what taskkill sends without /F), Ctrl-C and a console close end websServiceEvents and run
    websClose, which removes staged request data and stops CGI programs. Against the defect the window never
    receives a message and no control handler exists, so taskkill reports the process can only be terminated
    forcefully and the only way to stop the server skips its cleanup.

    Windows only: the message pump and the control handler have no equivalent elsewhere.
 */
import { tget, tskip, ttrue } from '@embedthis/testme'
import { execFileSync } from 'node:child_process'
import { createServer, connect } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

async function request(port: number, raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port})
        let data = ''
        socket.setTimeout(3000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

if (process.platform != 'win32') {
    tskip('Windows only')
} else {
    let port = Number(tget('TM_PRIVATE_PORT') || 0)
    if (!port) {
        const probe = createServer()
        await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
        port = (probe.address() as any).port
        await new Promise<void>(resolve => probe.close(() => resolve()))
    }

    const server = startServer(join(dir, '../build/bin/goahead.exe'),
        ['--no-strict', '--log', 'stdout:1', '--route', String(tget('TM_ROUTES')), 'web', `127.0.0.1:${port}`],
        {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']})
    let output = ''
    server.stdout.on('data', data => output += data.toString())
    server.stderr.on('data', data => output += data.toString())
    try {
        let ready = false
        for (let i = 0; i < 50 && !ready; i++) {
            ready = (await request(port, 'GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n'))
                .includes(' 200 ')
            if (!ready) await sleep(50)
        }
        ttrue(ready, 'private test server started')

        /*
            Without /F taskkill posts WM_CLOSE to the process's top-level windows rather than terminating it.
         */
        let exited = new Promise<boolean>(resolve => server.once('exit', () => resolve(true)))
        let killed = ''
        try {
            killed = execFileSync('taskkill', ['/PID', String(server.pid)], {encoding: 'utf8', stdio: 'pipe'})
        } catch (error: any) {
            killed = String(error.stdout || '') + String(error.stderr || '')
        }
        let stopped = await Promise.race([exited, sleep(8000).then(() => false)])
        ttrue(stopped, `the server must stop within 8 seconds of a close request: ${killed.trim()}`)
        if (stopped) {
            ttrue(server.exitCode === 0, `the server must exit through main, got code ${server.exitCode}`)
            ttrue(output.includes('Instructed to exit'), 'the server must run its shutdown path')
        }
    } finally {
        await stopServer(server)
    }
}
