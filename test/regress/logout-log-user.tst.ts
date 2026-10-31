/*
    logout-log-user.tst - A form logout names the user it logs out.

    Authentication outcomes are logged at the default level so an operator can attribute them. The logout action
    runs on a public route, so the request carries no username of its own: the identity being logged out is the
    one recorded in the session. The logout line must name that user, not "-", or the audit trail records a
    logout that nobody performed.
 */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
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

const get = (path: string, extra = '') =>
    `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n${extra}\r\n`
const post = (path: string, form: string, extra = '') =>
    `POST ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n${extra}` +
    `Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ${form.length}\r\n\r\n${form}`

async function withServer(body: (port: number, lines: () => string[]) => Promise<void>) {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    const server = startServer(join(dir, '../build/bin/goahead'),
        ['--no-strict', '--log', 'stderr:0', '--route', String(tget('TM_ROUTES')), 'web', `127.0.0.1:${port}`],
        {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']})
    let logs = ''
    server.stdout.on('data', data => logs += data.toString())
    server.stderr.on('data', data => logs += data.toString())
    server.on('error', error => ttrue(false, String(error)))
    try {
        let ready = false
        for (let i = 0; i < 50; i++) {
            if ((await request(port, get('/index.html'))).includes(' 200 ')) {
                ready = true
                break
            }
            await sleep(50)
        }
        ttrue(ready, 'private test server started')
        const mark = logs.length
        await body(port, () => logs.slice(mark).split(/\r?\n/).filter(line => line.includes('Authentication ')))
    } finally {
        await stopServer(server)
    }
}

//  Run one request and return the authentication lines it added
async function outcome(port: number, lines: () => string[], raw: string): Promise<[string, string[]]> {
    const before = lines().length
    const response = await request(port, raw)
    await sleep(100)
    return [response, lines().slice(before)]
}

if (!thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_LOGGING')) {
    tskip('Authentication or logging is not enabled')
} else {
    await withServer(async (port, lines) => {
        let [response, added] = await outcome(port, lines, post('/action/login', 'username=joshua&password=pass1'))
        ttrue(added.length === 1 && added[0].includes('succeeded'), 'the login is logged: ' + added[0])
        const match = response.match(/(-goahead-session-=[^;]*)/)
        ttrue(match != null, 'the login issues a session cookie')
        const cookie = match ? match[1] : ''

        //  The logout of a live session names the session's user
        ;[response, added] = await outcome(port, lines, post('/action/logout', '', `Cookie: ${cookie}\r\n`))
        ttrue(response.includes(' 302 '), 'the logout redirects')
        ttrue(added.length === 1, 'a form logout writes one line at level 0')
        ttrue((added[0] || '').includes('Authentication logout for user "joshua"'),
            'the logout line names the user being logged out: ' + added[0])

        //  The session is gone server-side
        ;[response, added] = await outcome(port, lines, get('/auth/form/index.html', `Cookie: ${cookie}\r\n`))
        ttrue(response.includes(' 302 ') && response.includes('/auth/form/login.html'), 'the session is logged out')

        //  A logout with no session has no user to name
        ;[response, added] = await outcome(port, lines, post('/action/logout', ''))
        ttrue(added.length === 1 && (added[0] || '').includes('logout for user "-"'),
            'a logout without a session names no user: ' + added[0])
    })
}
