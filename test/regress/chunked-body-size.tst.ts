/* A chunked body larger than the input buffer is accepted, as it is with Content-Length */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function post(body: string, chunked: boolean): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let head = 'POST /action/showTest HTTP/1.1\r\nHost: ' + ADDR + '\r\nConnection: close\r\n' +
        'Content-Type: application/x-www-form-urlencoded\r\n'
    if (chunked) {
        let data = ''
        for (let i = 0; i < body.length; i += 1000) {
            let piece = body.slice(i, i + 1000)
            data += piece.length.toString(16) + '\r\n' + piece + '\r\n'
        }
        await s.write(head + 'Transfer-Encoding: chunked\r\n\r\n' + data + '0\r\n\r\n')
    } else {
        await s.write(head + 'Content-Length: ' + body.length + '\r\n\r\n' + body)
    }
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function status(raw: string): number {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

let body = 'probe=' + 'x'.repeat(5000) + '&tail=end'
let raw = await post(body, false)
ttrue(status(raw) == 200, `a 5 KB form body with Content-Length is accepted, got ${status(raw)}`)

raw = await post(body, true)
ttrue(status(raw) == 200, `the same body chunked must be accepted, got ${status(raw)}`)
tcontains(raw, 'tail=end', 'and delivered whole')
