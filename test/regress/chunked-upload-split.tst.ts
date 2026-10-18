/* A chunked upload must complete when part lines straddle chunks and reads. */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const BOUNDARY = 'xYzZY'
const CONTENT = 'The quick brown fox jumps over the lazy dog.\n'.repeat(40)

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Upload support not enabled')
} else {
    let body = '--' + BOUNDARY + '\r\n' +
        'Content-Disposition: form-data; name="name"\r\n\r\nJohn Smith\r\n' +
        '--' + BOUNDARY + '\r\n' +
        'Content-Disposition: form-data; name="myfile"; filename="split.txt"\r\n' +
        'Content-Type: text/plain\r\n\r\n' + CONTENT + '\r\n' +
        '--' + BOUNDARY + '--\r\n'
    let chunked = ''
    for (let i = 0; i < body.length; i += 7) {
        let piece = body.slice(i, i + 7)
        chunked += piece.length.toString(16) + '\r\n' + piece + '\r\n'
    }
    chunked += '0\r\n\r\n'

    let s = new Socket
    s.connect(ADDR)
    await s.write('POST /action/uploadTest HTTP/1.1\r\nHost: x\r\nConnection: close\r\n' +
        'Content-Type: multipart/form-data; boundary=' + BOUNDARY + '\r\nTransfer-Encoding: chunked\r\n\r\n' + chunked)
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    let raw = buf.toString()
    ttrue(/^HTTP\/1\.1 200 /.test(raw), `the chunked upload must complete, got [${raw.slice(0, 40)}]`)
    ttrue(raw.contains('FILE_CLIENT_FILENAME_myfile=split.txt'), 'the file must be received')
    ttrue(raw.contains('FILE_SIZE_myfile=' + CONTENT.length), `the whole file must be received`)
    ttrue(raw.contains('name=John Smith'), 'the form field must be received')
}
