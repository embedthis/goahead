/* Behind a trusted TLS-terminating proxy the client's scheme drives routing and responses */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, '..')
const ROUTES = String(tget('TM_ROUTES'))
const ENDPOINT = '127.0.0.1:18981'
const URL = `http://${ENDPOINT}`

async function request(path: string, init: RequestInit = {}): Promise<Response> {
    return await fetch(URL + path, { redirect: 'manual', ...init })
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

    let response = await request('/secure/index.html', { headers: { 'X-Forwarded-Proto': 'https' } })
    ttrue(response.status != 302, `a request forwarded as https must not be redirected to https, got ${response.status}`)

    response = await request('/secure/index.html')
    ttrue(response.status == 302, `a plain http request must be redirected, got ${response.status}`)
    ttrue((response.headers.get('location') || '').startsWith('https://'), 'to https')

    if (thas('ME_GOAHEAD_AUTH')) {
        let login = { method: 'POST', body: 'username=joshua&password=pass1',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded', 'Host': 'dev.example',
            'Origin': 'https://dev.example', 'X-Forwarded-Proto': 'https' } }
        response = await request('/action/login', login)
        ttrue(response.status == 302, `a login forwarded as https must succeed, got ${response.status}`)
        ttrue((response.headers.get('set-cookie') || '').includes('secure'), 'and its session cookie must be secure')
        ttrue((response.headers.get('location') || '').startsWith('https://'), 'and it must redirect to https')

        login.headers['X-Forwarded-Proto'] = 'http'
        login.headers['Origin'] = 'http://dev.example'
        response = await request('/action/login', login)
        ttrue(response.status == 302, `a login forwarded as http must succeed, got ${response.status}`)
        ttrue(!(response.headers.get('set-cookie') || '').includes('secure'), 'and its session cookie must not be secure')
    } else {
        tskip('Authentication is not in this build')
    }
} finally {
    await stopServer(server)
}
