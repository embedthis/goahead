/*
    upload-part-state.tst - Each multipart part is described by exactly one Content-Disposition of its own

    The part name and client filename lived in request state that was reset only when a Content-Disposition
    header arrived, so a part without one inherited the previous part's name and its data was joined into
    that field, and a second Content-Disposition in a file part dropped the filename while the staging file
    and its descriptor stayed open and orphaned, which then refused the next file part as a duplicate. The
    state is now reset at every delimiter, a part without a Content-Disposition is refused (RFC 7578 4.2),
    and a repeated Content-Disposition is refused before it can disturb the file. Starts a private
    goahead-test, whose uploadTest action lists the parts the server saw.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'upload-part-state.dir')
const ENDPOINT = '127.0.0.1:' + (process.env.UPLOAD_PART_STATE_PORT || '18988')
const BOUNDARY = 'PartState'

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

function part(headers: string[], data: string): string {
    return `--${BOUNDARY}\r\n` + headers.map(h => h + '\r\n').join('') + `\r\n${data}\r\n`
}

function disposition(params: string): string {
    return 'Content-Disposition: form-data; ' + params
}

async function post(body: string): Promise<{ status: number, text: string }> {
    let response = await fetch(`http://${ENDPOINT}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        body: body + `--${BOUNDARY}--\r\n`,
    })
    return { status: response.status, text: await response.text() }
}

/*
    No staged files may remain once the refusal is received.
 */
async function staged(): Promise<number> {
    return readdirSync(join(HOME, 'tmp')).length
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

    //  A part without a Content-Disposition is refused, not joined into the previous field
    let r = await post(part([disposition('name="t"')], 'one') + part([], 'two'))
    ttrue(r.status == 400, `a part without a Content-Disposition must be refused, got ${r.status}`)
    ttrue(!r.text.includes('t=one two'), 'the data must not be joined into the previous field')

    //  Nor attributed to the previous file part's name
    r = await post(part([disposition('name="f"; filename="a.txt"')], 'hello') + part([], 'two'))
    ttrue(r.status == 400, `a bare part after a file part must be refused, got ${r.status}`)
    ttrue((await staged()) == 0, 'the refused request must leave nothing staged')
    rmSync(join(HOME, 'web', 'tmp', 'a.txt'), { force: true })

    //  A second Content-Disposition in a file part is refused at once, leaving no orphaned staging file
    r = await post(part([disposition('name="f"; filename="a.txt"'), disposition('name="t"')], 'hello') +
                   part([disposition('name="u"')], 'more'))
    ttrue(r.status == 400, `a repeated Content-Disposition must be refused, got ${r.status}`)
    ttrue(!r.text.includes('t=hello'), 'the file data must not become a text field')
    ttrue((await staged()) == 0, 'nothing may stay staged after the refusal')

    //  A later file part in the same request is not mistaken for a duplicate
    r = await post(part([disposition('name="f"; filename="a.txt"'), disposition('name="t"')], 'hello') +
                   part([disposition('name="g"; filename="b.txt"')], 'more'))
    ttrue(r.status == 400, `the request is refused at the repeated header, got ${r.status}`)
    ttrue((await staged()) == 0, 'nothing may stay staged after the refusal')

    //  Well formed parts, a file then a field with the same name, still work and the server is still up
    r = await post(part([disposition('name="f"; filename="a.txt"'), 'Content-Type: text/plain'], 'hello') +
                   part([disposition('name="f"')], 'text'))
    ttrue(r.status == 200, `well formed parts must be accepted, got ${r.status}`)
    ttrue(r.text.includes('CLIENT=a.txt\r\n') && r.text.includes('TYPE=text/plain\r\n') &&
          r.text.includes('\r\nf=text\r\n'), 'the file and the field must both be delivered')
    rmSync(join(HOME, 'web', 'tmp', 'a.txt'), { force: true })
    ttrue((await staged()) == 0, 'a completed request leaves nothing staged')
} finally {
    await stopServer(server)
    rmSync(HOME, { recursive: true, force: true })
}
