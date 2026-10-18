/*
    put-staging.tst - A PUT body becomes a document only when the file handler accepts it
 */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, tget, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync, rmSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const WEB = join(import.meta.dir, '..', 'web')

async function send(request: string, close: boolean = false): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(request)
    if (close) {
        s.close()
        return ''
    }
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

let body = 'probe=putbody'
let response = await send('PUT /putaction/showTest HTTP/1.0\r\nHost: localhost\r\n' +
    'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: ' + body.length + '\r\n\r\n' + body)
ttrue(status(response) == 200, `a PUT to an action must run the action, got ${status(response)}`)
tcontains(response, 'probe=putbody', 'and the action must receive the body')
ttrue(!existsSync(join(WEB, 'putaction', 'showTest')), 'and no document may be created')

let partial = join(WEB, 'tmp', 'put-staging-partial.txt')
rmSync(partial, { force: true })
await send('PUT /tmp/put-staging-partial.txt HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1000\r\n\r\npartial', true)
await new Promise(r => setTimeout(r, 500))
response = await send('GET /index.html HTTP/1.0\r\nHost: localhost\r\n\r\n')
ttrue(status(response) == 200, 'the server must serve the next request')
ttrue(!existsSync(partial), 'a PUT whose body never arrived must not leave a document')
rmSync(partial, { force: true })

let whole = join(WEB, 'tmp', 'put-staging-whole.txt')
rmSync(whole, { force: true })
body = 'the whole body'
response = await send('PUT /tmp/put-staging-whole.txt HTTP/1.0\r\nHost: localhost\r\nContent-Length: ' +
    body.length + '\r\n\r\n' + body)
ttrue(status(response) == 201, `a complete PUT must create the document, got ${status(response)}`)
ttrue(existsSync(whole) && readFileSync(whole, 'utf8') == body, 'with the whole body')
rmSync(whole, { force: true })
