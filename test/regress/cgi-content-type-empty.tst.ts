/* A CGI Content-Type header with no value is a malformed response, not a response with no usable type */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function cgi(switches: string): Promise<string> {
    let program = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + program + ' HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nSWITCHES: ' + switches + '\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function headers(response: string): string {
    return response.slice(0, response.indexOf('\r\n\r\n'))
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
} else {
    //  "Content-Type:" with nothing after the colon, and with only whitespace after it
    for (let empty of ['-H%20Content-Type:', '-H%20Content-Type:%2520%2520']) {
        let raw = await cgi(empty)
        let hdr = headers(raw)
        ttrue(/^HTTP\/1\.1 500 /.test(hdr),
            `${empty}: an empty Content-Type is a malformed CGI response, got [${hdr.split('\r\n')[0]}]`)
        ttrue(!/^content-type:[ \t]*$/im.test(hdr), `${empty}: no empty Content-Type header may be sent`)
        ttrue(!raw.contains('<HTML>'), `${empty}: none of the program's body may be forwarded`)
    }

    //  Control: a Content-Type with a value is the response's type
    let raw = await cgi('-H%20Content-Type:%2520text/plain')
    let hdr = headers(raw)
    ttrue(/^HTTP\/1\.1 200 /.test(hdr), 'a Content-Type with a value is forwarded')
    ttrue(/^content-type: text\/plain$/im.test(hdr), 'the Content-Type value must be forwarded')
}
