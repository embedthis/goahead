/* CGI environment values must never enter diagnostics, including debug logs. */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { createServer, connect } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const dir = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))
async function request(port: number, path: string, headers = ''): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: '127.0.0.1', port})
        let data = ''
        socket.setTimeout(3000, () => socket.destroy())
        socket.on('connect', () => socket.write(`GET ${path} HTTP/1.0\r\nHost: localhost\r\n${headers}\r\n`))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}
if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is disabled')
} else if (!thas('ME_GOAHEAD_LOGGING')) {
    tskip('Logging is disabled')
} else {
    for (const level of [0, 5]) {
        const probe = createServer()
        await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
        const port = (probe.address() as any).port
        await new Promise<void>(resolve => probe.close(() => resolve()))
        const server = startServer(join(dir, '../build/bin/goahead'),
            ['--no-strict', '--log', `stderr:${level}`, '--route', ROUTES, 'web', `127.0.0.1:${port}`],
            {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']})
        let logs = ''
        server.stdout.on('data', data => logs += data.toString())
        server.stderr.on('data', data => logs += data.toString())
        server.on('error', error => ttrue(false, String(error)))
        try {
            let ready = false
            for (let i = 0; i < 50; i++) {
                if ((await request(port, '/index.html')).includes(' 200 ')) { ready = true; break }
                await sleep(50)
            }
            ttrue(ready, 'private CGI test server started')
            const response = await request(port, '/cgi-bin/cgitest?secret=env-query-canary',
                'Cookie: session=env-cookie-canary\r\nAuthorization: Basic ZW52LWF1dGgtY2FuYXJ5\r\n')
            ttrue(response.includes(' 200 '), 'CGI still executes')
            await sleep(50)
            const envLogs = logs.split('\n').filter(line => line.includes('Env[')).join('\n')
            if (level === 0) ttrue(envLogs.length === 0, 'no environment logging at default verbosity')
            else ttrue(envLogs.includes('QUERY_STRING'), 'debug logs retain variable names')
            for (const secret of ['env-query-canary', 'env-cookie-canary', 'ZW52LWF1dGgtY2FuYXJ5']) {
                ttrue(!envLogs.includes(secret), 'environment diagnostics omit values')
            }
        } finally {
            await stopServer(server)
        }
    }
}
