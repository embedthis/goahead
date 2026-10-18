/* A multipart body without its closing delimiter fails at end of input */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { readdirSync } from 'node:fs'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const BOUNDARY = 'XYZ'
const STAGING = '../tmp'

async function exchange(payload: string): Promise<{raw: string, elapsed: number}> {
    let started = Date.now()
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return {raw: response.toString(), elapsed: Date.now() - started}
}

function head(): string {
    return 'POST /action/uploadTest HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n' +
           'Content-Type: multipart/form-data; boundary=' + BOUNDARY + '\r\n'
}

function lengthPost(body: string): string {
    return head() + 'Content-Length: ' + body.length + '\r\n\r\n' + body
}

function chunkedPost(body: string): string {
    return head() + 'Transfer-Encoding: chunked\r\n\r\n' + body.length.toString(16) + '\r\n' + body + '\r\n0\r\n\r\n'
}

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function staged(): number {
    return readdirSync(STAGING).filter((name) => name.startsWith('tmp-') && name.endsWith('.tmp')).length
}

if (thas('ME_GOAHEAD_UPLOAD')) {
    let baseline = staged()
    let truncated = '--XYZ\r\nContent-Disposition: form-data; name="f"; filename="f.txt"\r\n\r\nPARTIAL'

    for (let [name, request] of [['Content-Length', lengthPost(truncated)], ['chunked', chunkedPost(truncated)]]) {
        let result = await exchange(request)
        ttrue(status(result.raw) == '400', `a ${name} body missing its close delimiter must get 400, got ` +
              status(result.raw))
        ttrue(result.elapsed < 5000, `the refusal must not wait for the request timeout, took ${result.elapsed} ms`)
        ttrue(staged() == baseline, 'the staged file must be removed')
    }

    let field = '--XYZ\r\nContent-Disposition: form-data; name="field"\r\n\r\nPARTIAL'
    let result = await exchange(lengthPost(field))
    ttrue(status(result.raw) == '400', `a text part missing its delimiter must get 400, got ${status(result.raw)}`)

    let complete = '--XYZ\r\nContent-Disposition: form-data; name="field"\r\n\r\nvalue\r\n--XYZ--'
    result = await exchange(lengthPost(complete))
    ttrue(status(result.raw) == '200', `a close delimiter without a final CRLF must complete the body, got ` +
          status(result.raw))
    ttrue(result.raw.contains('field=value'), 'the part before the close delimiter must be parsed')
}
