/* Behind a trusted TLS-terminating proxy the Origin check uses the client's scheme */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawn } from 'node:child_process'
import { join } from 'node:path'

const HOME = join(import.meta.dir, '..')
//  The route table assembled for this build's feature set
const ROUTES = String(tget('TM_ROUTES'))
const ENDPOINT = '127.0.0.1:18982'
const URL = `http://${ENDPOINT}`
const BASIC = 'Basic ' + Buffer.from('joshua:pass1').toString('base64')

async function post(path: string, origin: string | null, proto: string, extra: Record<string, string> = {}) {
    let headers: Record<string, string> = { 'Content-Type': 'application/x-www-form-urlencoded', 'Host': 'dev.example',
        'X-Forwarded-Proto': proto, ...extra }
    if (origin) {
        headers['Origin'] = origin
    }
    let body = path == '/action/login' ? 'username=joshua&password=pass1' : 'probe=1'
    return await fetch(URL + path, { method: 'POST', redirect: 'manual', headers, body })
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not in this build')
} else {
    let log = ''
    let server = spawn('goahead-test', ['--proxy', '127.0.0.1', '--route', ROUTES, '--auth', 'auth.txt', 'web',
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

        for (let [path, extra, ok] of [['/action/login', {}, 302], ['/basicaction/showTest', { Authorization: BASIC }, 200]]) {
            let response = await post(path, 'https://dev.example', 'https', extra)
            ttrue(response.status == ok, `${path}: a same-site request behind the proxy must pass, got ${response.status}`)
            response = await post(path, 'https://evil.example', 'https', extra)
            ttrue(response.status == 403, `${path}: a request from another site must get 403, got ${response.status}`)
            response = await post(path, 'http://dev.example', 'https', extra)
            ttrue(response.status == 403, `${path}: an http Origin for an https client must get 403, got ${response.status}`)
        }
        let response = await post('/basicaction/showTest', null, 'https', { Authorization: BASIC, 'Sec-Fetch-Site': 'cross-site' })
        ttrue(response.status == 403, `with Origin stripped, Sec-Fetch-Site: cross-site must get 403, got ${response.status}`)
        response = await post('/basicaction/showTest', null, 'https', { Authorization: BASIC, 'Sec-Fetch-Site': 'same-origin' })
        ttrue(response.status == 200, `with Origin stripped, Sec-Fetch-Site: same-origin must pass, got ${response.status}`)
    } finally {
        server.kill()
    }
}
