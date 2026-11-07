/* Behind a trusted proxy a redirect names the Host the proxy preserved, not the backend interface */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const PORT = Number(tget('TM_PROXY_REDIRECT_PORT') || 18970)
const ENDPOINT = `127.0.0.1:${PORT}`
const URL = `http://${ENDPOINT}`

async function request(path: string, init: RequestInit = {}): Promise<Response> {
    let response = await fetch(URL + path, { redirect: 'manual', ...init })
    await response.text()
    return response
}

/* A raw request, for an HTTP/1.0 message without a Host header */
function raw(message: string): Promise<string> {
    return new Promise((resolve) => {
        let data = ''
        let socket = connect(PORT, '127.0.0.1', () => socket.write(message))
        socket.on('data', (chunk) => data += chunk)
        socket.on('close', () => resolve(data))
        socket.on('error', () => resolve(data))
    })
}

let log = ''
let server = startServer('goahead-test', ['--proxy', '127.0.0.1', '--route', ROUTES, '--auth', 'auth.txt', 'web',
    `http://${ENDPOINT}`], { cwd: HOME })
server.stdout.on('data', (data) => log += data)
server.stderr.on('data', (data) => log += data)
try {
    let up = false
    for (let i = 0; i < 50 && !up; i++) {
        await new Promise(r => setTimeout(r, 100))
        try {
            await fetch(`${URL}/index.html`)
            up = true
        } catch {}
    }
    ttrue(up, `the server must start: ${log}`)

    /* The https upgrade of a protocol=http route, forwarded as plain http by the proxy */
    let response = await request('/secure/index.html', { headers: { 'Host': 'dev.example', 'X-Forwarded-Proto': 'http' } })
    ttrue(response.status == 302, `a plain http request behind the proxy must be redirected, got ${response.status}`)
    let location = response.headers.get('location') || ''
    ttrue(location == 'https://dev.example/secure/index.html',
        `the upgrade must name the Host the proxy preserved, got ${location}`)

    /* A directory redirect, forwarded as https from a proxy on a non-default port */
    response = await request('/dir', { headers: { 'Host': 'dev.example:8443', 'X-Forwarded-Proto': 'https' } })
    ttrue(response.status == 302, `a directory must redirect, got ${response.status}`)
    location = response.headers.get('location') || ''
    ttrue(location == 'https://dev.example:8443/dir/index.html',
        `the directory redirect must keep the client's host and port, got ${location}`)
    ttrue(!location.includes('127.0.0.1'), 'the backend interface address must not reach the client')

    /* The redirect that follows a form login */
    if (thas('ME_GOAHEAD_AUTH')) {
        response = await request('/action/login', { method: 'POST', body: 'username=joshua&password=pass1',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'Host': 'dev.example',
            'Origin': 'https://dev.example', 'X-Forwarded-Proto': 'https' } })
        ttrue(response.status == 302, `a login behind the proxy must succeed, got ${response.status}`)
        location = response.headers.get('location') || ''
        ttrue(location == 'https://dev.example/auth/form/index.html',
            `the login redirect must name the Host the proxy preserved, got ${location}`)
    } else {
        tskip('Authentication is not in this build')
    }

    /* Without a forwarded scheme the request is not proxied: the server still names itself as before */
    response = await request('/dir', { headers: { 'Host': 'dev.example' } })
    ttrue(response.status == 302, `a direct directory request must redirect, got ${response.status}`)
    location = response.headers.get('location') || ''
    ttrue(location == `http://127.0.0.1:${PORT}/dir/index.html`,
        `a request the proxy did not mark keeps the interface address and listener port, got ${location}`)

    /* An HTTP/1.0 request without a Host header has no host to name: the server still names itself */
    let text = await raw('GET /dir HTTP/1.0\r\nX-Forwarded-Proto: https\r\n\r\n')
    let match = text.match(/[Ll]ocation: ([^\r\n]*)/)
    location = match ? match[1] : ''
    ttrue(location == 'https://127.0.0.1/dir/index.html',
        `without a Host header the redirect must fall back to the interface address, got ${location}`)
} finally {
    await stopServer(server)
}
