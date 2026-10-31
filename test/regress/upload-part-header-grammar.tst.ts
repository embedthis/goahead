/*
    Refuse malformed multipart headers, embedded NULs, repeated names and extended parameters
    with 400 before staging a part.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'upload-part-header-grammar.dir')
const ENDPOINT = '127.0.0.1:' + (process.env.UPLOAD_HEADER_GRAMMAR_PORT || '18995')
const BOUNDARY = 'HeaderGrammar'

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

/*
    One part. The delimiter line is a parameter so a test can misspell it.
 */
function part(headers: string[], data: string, delimiter = `--${BOUNDARY}`): string {
    return `${delimiter}\r\n` + headers.map(h => h + '\r\n').join('') + `\r\n${data}\r\n`
}

function disposition(params: string): string {
    return 'Content-Disposition: form-data; ' + params
}

async function post(body: string): Promise<{ status: number, text: string }> {
    let response = await fetch(`http://${ENDPOINT}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        //  latin1 keeps a NUL and every other byte as written
        body: Buffer.from(body + `--${BOUNDARY}--\r\n`, 'latin1'),
    })
    return { status: response.status, text: await response.text() }
}

function staged(): number {
    return readdirSync(join(HOME, 'tmp')).length
}

function stored(name: string): boolean {
    let path = join(HOME, 'web', 'tmp', name)
    let found = readdirSync(join(HOME, 'web', 'tmp')).includes(name)
    rmSync(path, { force: true })
    return found
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

    //  A folded continuation line is not a header of its own, and is not silently dropped either
    let r = await post(part([disposition('name="t"'), ' ; filename="x.txt"'], 'one'))
    ttrue(r.status == 400, `a folded part header must be refused, got ${r.status}`)
    ttrue(!r.text.includes('t=one'), 'the part must not be delivered as a text field')
    ttrue(!stored('x.txt') && staged() == 0, 'nothing may be stored or staged after the refusal')
    r = await post(part([disposition('name="t"'), '\t; filename="x.txt"'], 'one'))
    ttrue(r.status == 400, `a tab-folded part header must be refused, got ${r.status}`)
    ttrue(!stored('x.txt') && staged() == 0, 'nothing may be stored or staged after the refusal')

    //  Whitespace between the field name and the colon, and a field name with no colon
    r = await post(part(['Content-Disposition : form-data; name="t"'], 'one'))
    ttrue(r.status == 400, `whitespace before the colon must be refused, got ${r.status}`)
    r = await post(part(['Content-Disposition form-data; name="t"'], 'one'))
    ttrue(r.status == 400, `a header line without a colon must be refused, got ${r.status}`)
    ttrue(!r.text.includes('t=one'), 'the part must not be delivered as a text field')

    //  A NUL inside a header line and inside a delimiter line
    r = await post(part([disposition('name="t"') + '\0; filename="x.txt"'], 'one'))
    ttrue(r.status == 400, `a NUL in a part header must be refused, got ${r.status}`)
    ttrue(!r.text.includes('t=one') && !stored('x.txt') && staged() == 0, 'the part must not be delivered at all')
    r = await post(part([disposition('name="t"')], 'one', `--${BOUNDARY}\0junk`))
    ttrue(r.status == 400, `a NUL in a delimiter line must be refused, got ${r.status}`)
    ttrue(!r.text.includes('t=one'), 'the line is not a delimiter')

    //  One stray byte after a quoted value
    r = await post(part([disposition('name="t"x')], 'one'))
    ttrue(r.status == 400, `a byte after a quoted value must be refused, got ${r.status}`)
    r = await post(part([disposition('name="t"x; filename="a.txt"')], 'one'))
    ttrue(r.status == 400, `a byte after a quoted value must be refused before a file is staged, got ${r.status}`)
    ttrue(!stored('a.txt') && staged() == 0, 'nothing may be stored or staged after the refusal')

    //  A repeated name parameter
    r = await post(part([disposition('name="a"; filename="f.txt"; name="b"')], 'one'))
    ttrue(r.status == 400, `a repeated name parameter must be refused, got ${r.status}`)
    ttrue(!r.text.includes('FILE: b') && !stored('f.txt') && staged() == 0, 'the file must not be staged under the second name')

    //  RFC 2231 and RFC 5987 extended parameters, which form-data prohibits
    r = await post(part([disposition("name=\"a\"; filename*=UTF-8''b.txt")], 'one'))
    ttrue(r.status == 400, `an extended filename parameter must be refused, got ${r.status}`)
    ttrue(!r.text.includes('a=one'), 'the part must not be delivered as a text field')
    r = await post(part([disposition('name*0="a"; name*1="b"')], 'one'))
    ttrue(r.status == 400, `a continued name parameter must be refused, got ${r.status}`)

    //  Well formed parts still work: no space after the colon, a lower-case field name, a file and a field
    r = await post(part(['content-disposition:form-data; name="f"; filename="a.txt"', 'Content-Type:text/plain'], 'hello') +
                   part([disposition('name="t"')], 'text'))
    ttrue(r.status == 200, `well formed parts must be accepted, got ${r.status}`)
    ttrue(r.text.includes('CLIENT=a.txt\r\n') && r.text.includes('TYPE=text/plain\r\n') &&
          r.text.includes('\r\nt=text\r\n'), 'the file and the field must both be delivered')
    ttrue(stored('a.txt'), 'the action stored the upload under its client name')
    ttrue(staged() == 0, 'a completed request leaves nothing staged')
} finally {
    await stopServer(server)
    rmSync(HOME, { recursive: true, force: true })
}
