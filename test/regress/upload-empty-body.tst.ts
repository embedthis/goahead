/*
    upload-empty-body.tst - A multipart request without a body is refused before its handler runs

    A multipart POST with Content-Length: 0, or none, was dispatched straight to its handler: the upload filter
    runs only while a body arrives, so wp->files was still the -1 placeholder and the documented iteration over
    websGetUpload() hit the hash library's handle assertion. goahead-test aborts on an assertion, so one request
    stopped the test server; a release build logs the assertion for every such request. An empty multipart body
    is malformed (RFC 2046 5.1.1 requires a delimiter), and is now refused with 400 like the chunked spelling
    already was. Starts a private goahead-test, whose uploadTest action is the documented iteration.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'upload-empty-body.dir')
const ENDPOINT = '127.0.0.1:' + (process.env.UPLOAD_EMPTY_BODY_PORT || '18991')
const BOUNDARY = 'EmptyBody'

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Uploads are not in this build')
    process.exit(0)
}

rmSync(HOME, { recursive: true, force: true })
mkdirSync(join(HOME, 'web', 'tmp'), { recursive: true })
mkdirSync(join(HOME, 'tmp'), { recursive: true })
writeFileSync(join(HOME, 'web', 'index.html'), '<html>INDEX</html>\n')
writeFileSync(join(HOME, 'route.txt'), 'route uri=/action/uploadTest upload=true handler=action\nroute uri=/\n')
writeFileSync(join(HOME, 'auth.txt'), '')

const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []
let log = ''
let server = startServer('goahead-test', ['--route', 'route.txt', ...AUTH, 'web', ENDPOINT], { cwd: HOME })
server.stdout?.on('data', (data) => log += data)
server.stderr?.on('data', (data) => log += data)

async function post(contentType: string, body: string): Promise<number> {
    try {
        let response = await fetch(`http://${ENDPOINT}/action/uploadTest`, {
            method: 'POST',
            headers: { 'Content-Type': contentType },
            body,
        })
        await response.text()
        return response.status
    } catch {
        return 0
    }
}

async function alive(): Promise<boolean> {
    try {
        return (await fetch(`http://${ENDPOINT}/index.html`)).status == 200
    } catch {
        return false
    }
}

try {
    let index: Response | null = null
    for (let i = 0; i < 50 && !index; i++) {
        await new Promise(r => setTimeout(r, 100))
        try {
            index = await fetch(`http://${ENDPOINT}/index.html`)
        } catch {}
    }
    ttrue(index != null && index.status == 200, `the server must start: ${log}`)

    //  Content-Length: 0 with a multipart content type
    let status = await post(`multipart/form-data; boundary=${BOUNDARY}`, '')
    ttrue(status == 400, `an empty multipart body must be refused, got ${status}`)
    ttrue(await alive(), `the server must survive an empty multipart body: ${log}`)

    //  A POST to the upload route that is not multipart at all
    status = await post('application/x-www-form-urlencoded', 'a=1')
    ttrue(status == 200, `a form POST to the upload route must be answered, got ${status}`)
    ttrue(await alive(), `the server must survive a form POST to the upload route: ${log}`)

    //  A real upload still works
    let body = `--${BOUNDARY}\r\nContent-Disposition: form-data; name="f"; filename="ok.txt"\r\n\r\nok\r\n--${BOUNDARY}--\r\n`
    let response = await fetch(`http://${ENDPOINT}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        body,
    })
    let text = await response.text()
    ttrue(response.status == 200 && text.includes('CLIENT=ok.txt\r\n'), 'an upload must still be accepted')
} finally {
    await stopServer(server)
    rmSync(HOME, { recursive: true, force: true })
}
