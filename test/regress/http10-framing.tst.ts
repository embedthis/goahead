/*
    http10-framing.tst - An HTTP/1.0 client must never be sent a chunked response
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function request(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let buf = new ByteArray(131072, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    return buf.toString()
}

function headers(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return end < 0 ? raw : raw.slice(0, end)
}

function body(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return end < 0 ? '' : raw.slice(end + 4)
}

/*
    Check unknown-length JST responses: close-delimited for HTTP/1.0, chunked for HTTP/1.1.
 */
if (thas('ME_GOAHEAD_JAVASCRIPT')) {
    let raw = await request('GET /test.jst HTTP/1.0\r\n\r\n')
    ttrue(raw.contains('200 OK'), 'the HTTP/1.0 JST request must be served')
    ttrue(!headers(raw).toLowerCase().contains('transfer-encoding'),
          `an HTTP/1.0 response must not be chunked, got headers [${headers(raw)}]`)

    /*
        Reject a leading chunk-size line in the HTTP/1.0 body.
     */
    ttrue(!body(raw).match(/^[0-9a-fA-F]+\r\n/),
          'an HTTP/1.0 body must not begin with a chunk size line')

    raw = await request('GET /test.jst HTTP/1.1\r\nHost: ' + uri.host + '\r\nConnection: close\r\n\r\n')
    ttrue(raw.contains('200 OK'), 'the HTTP/1.1 JST request must be served')
    ttrue(headers(raw).toLowerCase().contains('transfer-encoding: chunked'),
          'an HTTP/1.1 response of unknown length must still be chunked')
}

if (thas('ME_GOAHEAD_CGI') && process.platform != 'win32') {
    let raw = await request('GET /cgi-bin/cgitest HTTP/1.0\r\n\r\n')
    ttrue(raw.contains('200 OK'), 'the HTTP/1.0 CGI request must be served')
    ttrue(!headers(raw).toLowerCase().contains('transfer-encoding'),
          `an HTTP/1.0 CGI response must not be chunked, got headers [${headers(raw)}]`)
}

let raw = await request('GET /index.html HTTP/1.0\r\n\r\n')
ttrue(headers(raw).toLowerCase().contains('content-length'),
      'a static HTTP/1.0 response must still declare Content-Length')
ttrue(!headers(raw).toLowerCase().contains('transfer-encoding'),
      'a static HTTP/1.0 response must not be chunked')
