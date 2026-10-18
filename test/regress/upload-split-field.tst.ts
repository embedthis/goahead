/* A multipart text field split across reads is delivered whole */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const BOUNDARY = 'SplitFieldBoundary'

function status(raw: string): number {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Uploads are not in this build')
} else {
    let value = 'first-half-of-the-value-' + 'v'.repeat(40) + '-second-half-of-the-value'
    let body = '--' + BOUNDARY + '\r\n' +
        'Content-Disposition: form-data; name="note"\r\n\r\n' +
        value + '\r\n' +
        '--' + BOUNDARY + '--\r\n'
    let head = 'POST /action/uploadTest HTTP/1.0\r\nHost: ' + ADDR + '\r\n' +
        'Content-Type: multipart/form-data; boundary=' + BOUNDARY + '\r\n' +
        'Content-Length: ' + body.length + '\r\n\r\n'
    let split = body.indexOf('-second-half')

    let s = new Socket
    s.connect(ADDR)
    await s.write(head + body.slice(0, split))
    await new Promise(r => setTimeout(r, 300))
    await s.write(body.slice(split))
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    let raw = response.toString()

    ttrue(status(raw) == 200, `a field split across reads must be accepted, got ${status(raw)}`)
    tcontains(raw, 'note=' + value, 'and delivered whole')
}
