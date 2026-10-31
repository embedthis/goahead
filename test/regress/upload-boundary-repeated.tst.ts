/*
    Refuse repeated boundary parameters with 400 before reading any part.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'upload-boundary-repeated.dir')
const ENDPOINT = '127.0.0.1:' + (process.env.UPLOAD_BOUNDARY_REPEATED_PORT || '18996')

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

function body(boundary: string): string {
    return `--${boundary}\r\nContent-Disposition: form-data; name="t"\r\n\r\none\r\n--${boundary}--\r\n`
}

async function post(contentType: string, boundary: string): Promise<{ status: number, text: string }> {
    let response = await fetch(`http://${ENDPOINT}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': contentType },
        body: body(boundary),
    })
    return { status: response.status, text: await response.text() }
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

    //  Two boundary parameters, the body framed by the first
    let r = await post('multipart/form-data; boundary=First; boundary=Second', 'First')
    ttrue(r.status == 400, `a repeated boundary parameter must be refused, got ${r.status}`)
    ttrue(!r.text.includes('t=one'), 'no part may be delivered')

    //  The same, framed by the second
    r = await post('multipart/form-data; boundary=First; boundary=Second', 'Second')
    ttrue(r.status == 400, `a repeated boundary parameter must be refused whichever frames the body, got ${r.status}`)

    //  Case and quoting do not make the parameters distinct
    r = await post('multipart/form-data; boundary="First"; BOUNDARY=First', 'First')
    ttrue(r.status == 400, `a repeated boundary parameter in another spelling must be refused, got ${r.status}`)

    //  One boundary, with another parameter beside it, still works
    r = await post('multipart/form-data; charset=utf-8; boundary=Only', 'Only')
    ttrue(r.status == 200, `a single boundary parameter must be accepted, got ${r.status}`)
    ttrue(r.text.includes('\r\nt=one\r\n'), 'the field must be delivered')
    ttrue(readdirSync(join(HOME, 'tmp')).length == 0, 'nothing is left staged')
} finally {
    await stopServer(server)
    rmSync(HOME, { recursive: true, force: true })
}
