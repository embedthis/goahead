/*
    upload-disposition-quoted.tst - Content-Disposition parameters are parsed as RFC 2045 quoted-strings

    processUploadHeader split the Content-Disposition value on ";" before it looked at quotes, so a quoted
    name or filename ending at the first ";" and anything after it was read as further parameters. A text
    field whose name holds "; filename=" was staged as a file under a client-chosen name, a filename holding
    ";" was truncated, and a quoted-pair escape could not be expressed. Parameters are now tokenized with
    quoted-strings honoured and quoted-pairs decoded, so a part means the same to GoAhead as to an RFC parser.
    Starts a private goahead-test, whose uploadTest action lists the parts the server saw.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { existsSync, mkdirSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'upload-disposition-quoted.dir')
const ENDPOINT = '127.0.0.1:' + (process.env.UPLOAD_QUOTED_PORT || '18987')
const BOUNDARY = 'DispositionQuoted'

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

function part(disposition: string, data: string): string {
    return `--${BOUNDARY}\r\nContent-Disposition: form-data; ${disposition}\r\n\r\n${data}\r\n`
}

async function post(body: string): Promise<{ status: number, text: string }> {
    let response = await fetch(`http://${ENDPOINT}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        body: body + `--${BOUNDARY}--\r\n`,
    })
    return { status: response.status, text: await response.text() }
}

function stagedFiles(): string[] {
    return readdirSync(join(HOME, 'web', 'tmp'))
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

    //  A quoted name may hold ";": the part is the text field it declares, not a file
    let r = await post(part('name="a; filename=run.jst"', 'TEXT'))
    ttrue(r.status == 200, `a quoted name holding ";" must be accepted, got ${r.status}`)
    ttrue(r.text.includes('\r\na; filename=run.jst=TEXT\r\n'), 'the field must keep its whole quoted name')
    ttrue(!r.text.includes('FILE: '), 'a text field must not be staged as a file')
    ttrue(!existsSync(join(HOME, 'web', 'tmp', 'run.jst')), 'no file may be stored under the name inside the quotes')

    //  A quoted filename may hold ";" and is delivered whole
    r = await post(part('name="f"; filename="x;y.txt"', 'DATA'))
    ttrue(r.status == 200, `a quoted filename holding ";" must be accepted, got ${r.status}`)
    ttrue(r.text.includes('CLIENT=x;y.txt\r\n'), 'the client filename must be the whole quoted string')
    ttrue(r.text.includes('FILE_CLIENT_FILENAME_f=x;y.txt\r\n'), 'the part keeps the name it declares')
    rmSync(join(HOME, 'web', 'tmp', 'x;y.txt'), { force: true })

    //  A quoted-pair escapes a quote inside a name
    r = await post(part('name="a\\"b"', 'v'))
    ttrue(r.status == 200, `an escaped quote in a name must be accepted, got ${r.status}`)
    ttrue(r.text.includes('\r\na"b=v\r\n'), 'the quoted-pair must decode to the quote')

    //  Unquoted values and the form-data token still parse
    r = await post(part('name=plain; filename=plain.txt', 'd'))
    ttrue(r.status == 200 && r.text.includes('CLIENT=plain.txt\r\n'), 'an unquoted filename must still be accepted')
    rmSync(join(HOME, 'web', 'tmp', 'plain.txt'), { force: true })

    //  Malformed parameters are refused rather than guessed at
    for (let disposition of ['name="open', 'name="a"junk', 'name=a b']) {
        r = await post(part(disposition, 'v'))
        ttrue(r.status == 400, `[${disposition}] must be refused, got ${r.status}`)
    }

    //  The empty-name and duplicate-filename refusals are unchanged
    r = await post(part('name=""', 'v'))
    ttrue(r.status == 400, `an empty name must still be refused, got ${r.status}`)
    r = await post(part('name="f"; filename="one.dat"; filename="two.dat"', 'v'))
    ttrue(r.status == 400, `a repeated filename must still be refused, got ${r.status}`)
    ttrue(stagedFiles().length == 0, `refused parts must leave nothing stored: ${stagedFiles()}`)
    ttrue(readdirSync(join(HOME, 'tmp')).length == 0, 'refused parts must leave nothing staged')
} finally {
    await stopServer(server)
    rmSync(HOME, { recursive: true, force: true })
}
