/*
    cgi-status-sources.tst - A CGI's status is honoured wherever the CGI states it
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function cgi(response: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nSWITCHES: -r%20' + response +
                  '\r\nConnection: close\r\n\r\n')
    let raw = new ByteArray(65536, true)
    try {
        while ((await s.read(raw, -1)) != null) {}
    } catch {
    }
    s.close()
    return raw.toString()
}

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function headers(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return (end < 0 ? raw : raw.slice(0, end)).toLowerCase()
}

const SP = '%2520'
const CRLF = '%250d%250a'

if (thas('ME_GOAHEAD_CGI')) {
    for (let [code, reason] of [['401', 'Unauthorized'], ['403', 'Forbidden'], ['404', 'Not' + SP + 'Found']]) {
        let raw = await cgi('HTTP/1.1' + SP + code + SP + reason + CRLF +
                            'Content-Type:' + SP + 'text/plain' + CRLF + CRLF + 'denied')
        ttrue(status(raw) == code, `an NPH status line of ${code} must be honoured, got ${status(raw)}`)
    }

    let raw = await cgi('HTTP/1.1' + SP + '200' + SP + 'OK' + CRLF + 'Content-Type:' + SP + 'text/plain' +
                        CRLF + CRLF + 'fine')
    ttrue(status(raw) == '200', `an NPH 200 must be served as 200, got ${status(raw)}`)
    ttrue(raw.contains('fine'), 'the NPH body must be delivered')

    raw = await cgi('HTTP/1.1' + SP + 'abc' + CRLF + 'Content-Type:' + SP + 'text/plain' + CRLF + CRLF + 'x')
    ttrue(status(raw) == '500', `an unparseable NPH status line must yield 500, got ${status(raw)}`)

    raw = await cgi('Location:' + SP + '/index.html' + CRLF + CRLF)
    ttrue(status(raw) == '302', `a Location with no Status must be a 302, got ${status(raw)}`)
    ttrue(headers(raw).contains('location: /index.html'), 'the Location must be sent with the redirect')

    raw = await cgi('Location:' + SP + 'http://example.com/elsewhere' + CRLF + CRLF)
    ttrue(status(raw) == '302', `an absolute Location with no Status must be a 302, got ${status(raw)}`)

    raw = await cgi('Status:' + SP + '201' + CRLF + 'Location:' + SP + '/index.html' + CRLF + CRLF)
    ttrue(status(raw) == '201', `an explicit Status must win over the Location default, got ${status(raw)}`)

    raw = await cgi('Location:' + SP + '/index.html' + CRLF + 'Status:' + SP + '303' + CRLF + CRLF)
    ttrue(status(raw) == '303', `a Status after the Location must still win, got ${status(raw)}`)

    raw = await cgi('Content-Type:' + SP + 'text/plain' + CRLF + CRLF + 'plain')
    ttrue(status(raw) == '200', `a response with no status source must be 200, got ${status(raw)}`)
}
