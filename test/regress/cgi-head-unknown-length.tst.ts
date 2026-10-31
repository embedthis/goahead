/* HEAD on a CGI response of unknown length must not declare a Content-Length the equivalent GET does not send */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { teq, thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const PROGRAM = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    return buf.toString()
}

function request(method: string, switches: string, last: boolean): string {
    return method + ' ' + PROGRAM + ' HTTP/1.1\r\nHost: localhost\r\nSWITCHES: ' + switches + '\r\n' +
        (last ? 'Connection: close\r\n' : '') + '\r\n'
}

function firstHeaders(raw: string): string {
    return raw.slice(0, raw.indexOf('\r\n\r\n'))
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
} else {
    //  The CGI declares no length: GET is framed by chunking, so it has no Content-Length
    let get = firstHeaders(await exchange(request('GET', '-b%201', true)))
    ttrue(/^transfer-encoding: chunked$/im.test(get), 'the GET of an unknown-length CGI response is chunked')
    ttrue(!/^content-length:/im.test(get), 'the GET of an unknown-length CGI response declares no length')

    //  HEAD, pipelined with a GET so the framing is checked too
    let raw = await exchange(request('HEAD', '-b%201', false) + 'GET /alive.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
    let head = firstHeaders(raw)
    ttrue(/^HTTP\/1\.1 200 /.test(head), 'HEAD must be answered')
    ttrue(!/^content-length:/im.test(head),
        `HEAD must not declare a Content-Length the GET does not send, got [${(head.match(/^content-length:.*$/im) || [''])[0]}]`)
    let rest = raw.slice(raw.indexOf('\r\n\r\n') + 4)
    ttrue(/^HTTP\/1\.1 200 /.test(rest), 'HEAD must carry no body, so the next response follows its headers')
    teq((raw.match(/^HTTP\/1\.1 \d+/gm) || []).length, 2, 'exactly two responses')

    //  A declared length is still reported, as the GET would report it
    head = firstHeaders(await exchange(request('HEAD', '-L%204%20-B%204', true)))
    ttrue(/^content-length: 4$/im.test(head), 'HEAD must report the length the CGI declared')
}
