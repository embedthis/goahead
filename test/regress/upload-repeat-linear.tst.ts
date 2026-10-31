/*
    upload-repeat-linear.tst - Repeated multipart field names are joined in linear time

    A multipart text field was stored as each part ended, and a repeated name re-formatted the whole
    accumulated value for every repeat, so one request of a large field followed by hundreds of empty parts
    of the same name copied the value hundreds of times and held the single-threaded server for seconds.
    The same defect in urlencoded bodies was 10358. Text fields are now held until the body completes and
    stored once per name with a single sized join. Starts a private goahead-test, which raises the POST
    limit to 16 MB; the shipped 16 KB default makes the cost negligible, so the test needs the raised limit.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'upload-repeat-linear.dir')
const ENDPOINT = '127.0.0.1:' + (process.env.UPLOAD_REPEAT_PORT || '18989')
const BOUNDARY = 'RepeatLinear'
const REPEATS = 511                 // ME_GOAHEAD_LIMIT_PARAMS - 1

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

function part(name: string, data: string): string {
    return `--${BOUNDARY}\r\nContent-Disposition: form-data; name="${name}"\r\n\r\n${data}\r\n`
}

async function post(body: string, query = ''): Promise<{ status: number, text: string, elapsed: number }> {
    let started = Date.now()
    let response = await fetch(`http://${ENDPOINT}/action/uploadTest${query}`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        body: body + `--${BOUNDARY}--\r\n`,
    })
    let text = await response.text()
    return { status: response.status, text, elapsed: Date.now() - started }
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

    let value = 'x'.repeat(8 * 1024 * 1024)
    let control = await post(part('a', value))
    ttrue(control.status == 200, `the single field must be accepted, got ${control.status}`)
    let repeated = await post(part('a', value) + part('a', '').repeat(REPEATS))
    ttrue(repeated.status == 200, `the repeated field must be accepted, got ${repeated.status}`)
    ttrue(repeated.text.includes('\r\na=' + value + ' '.repeat(REPEATS) + '\r\n'),
          'the repeats must join the value in order')
    ttrue(repeated.elapsed <= Math.max(5 * control.elapsed, 1000),
          `repeated names must cost about as much as the value alone: ${repeated.elapsed} ms against ` +
          `${control.elapsed} ms`)

    //  Join order and the query-first rule are unchanged
    let r = await post(part('a', 'v') + part('b', '1') + part('a', 'w') + part('a', ''))
    ttrue(r.text.includes('\r\na=v w \r\n'), 'repeated values must join with spaces in order')
    ttrue(r.text.includes('\r\nb=1\r\n'), 'other fields are unchanged')
    //  A multipart field is stored before the query is parsed at dispatch, so the body value comes first
    r = await post(part('a', 'B'), '?a=Q')
    ttrue(r.text.includes('\r\na=B Q\r\n'), 'a name in the body and the query must still join')

    //  A field named like a server variable is still refused
    r = await post(part('f', 'x') + part('UPLOAD_DIR', '/'))
    ttrue(r.status == 200 && !r.text.includes('UPLOAD_DIR=/'), 'a reserved name must still be ignored')
} finally {
    await stopServer(server)
    rmSync(HOME, { recursive: true, force: true })
}
