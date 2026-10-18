/* A CGI header holding a control byte reaches the wire whole or not at all */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function cgitest(switches: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET /cgi-bin/cgitest HTTP/1.0\r\nSWITCHES: ' + switches + '\r\n\r\n')
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    return buf.toString()
}

function status(raw: string): number {
    let m = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return m ? parseInt(m[1]) : 0
}

function headerBlock(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return end < 0 ? raw : raw.slice(0, end + 2)
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not enabled')
} else {
    /*
        cgitest descapes -H once more than getArgv, so the control byte is double encoded
     */
    for (let byte of ['01', '0b', '7f']) {
        let raw = await cgitest('-H%20X-Reflect:%2520a%25' + byte + 'b')
        let head = headerBlock(raw)
        ttrue(status(raw) == 200, `cgitest must still answer, got ${status(raw)}`)
        ttrue(!head.toLowerCase().contains('x-reflect'), `a header holding 0x${byte} must not reach the wire`)
        ttrue(!head.contains('<HTML>') && !head.contains('<html>'), 'the CGI body must not be in the header block')
        ttrue(raw.split('\r\n\r\n').length >= 2 && head.split('\r\n').every(line => line == '' ||
            /^[!#$&'*+\-.^_`|~0-9A-Za-z]+: /.test(line) || /^HTTP\/1\.[01] /.test(line)),
            `every header line must be whole, got ${head}`)
    }

    //  A refused header that is not last must not merge into the next one
    let raw = await cgitest('-H%20X-Reflect:%2520a%2501b%20-h%201')
    ttrue(headerBlock(raw).toLowerCase().contains('\r\nx-cgi-0: a loo'), 'the header after a refused one must be intact')

    //  Control headers: -l and -c are descaped once
    raw = await cgitest('-l%20/home%01')
    ttrue(status(raw) == 500, `a control byte in Location must give 500, got ${status(raw)}`)
    ttrue(!raw.toLowerCase().contains('location:'), 'no Location may be sent')
    ttrue(!raw.contains('<HTML>') && !raw.contains('<html>'), 'none of the CGI output may be sent')

    raw = await cgitest('-c%20text/html%0b')
    ttrue(status(raw) == 500, `a control byte in Content-Type must give 500, got ${status(raw)}`)
    ttrue(!raw.contains('<HTML>') && !raw.contains('<html>'), 'none of the CGI output may be sent')
}
