/* Multipart names and values are literal, and the boundary is a parameter */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function post(contentType: string, body: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST /action/uploadTest HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n' +
                  'Content-Type: ' + contentType + '\r\nContent-Length: ' + body.length + '\r\n\r\n' + body)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function field(name: string, value: string): string {
    return '--XYZ\r\nContent-Disposition: form-data; name="' + name + '"\r\n\r\n' + value + '\r\n--XYZ--\r\n'
}

if (thas('ME_GOAHEAD_UPLOAD')) {
    const TYPE = 'multipart/form-data; boundary=XYZ'

    let raw = await post(TYPE, field('p', '100%25+a%41'))
    ttrue(raw.contains('\r\np=100%25+a%41\r\n'), 'a multipart value must arrive unchanged')

    raw = await post(TYPE, field('a%5Fb', 'x'))
    ttrue(raw.contains('\r\na%5Fb=x\r\n'), 'a multipart name must not be decoded')
    ttrue(!raw.contains('\r\na_b='), 'an encoded name must not become the decoded name')

    raw = await post(TYPE, field('FILE%5FFILENAME_f', '/etc/passwd'))
    ttrue(!raw.contains('FILE_FILENAME_f='), 'an encoded name must not spell an upload variable')

    for (let type of ['multipart/form-data; boundary="XYZ"; charset=utf-8',
                      'multipart/form-data; charset=utf-8; boundary=XYZ', 'multipart/form-data; BOUNDARY=XYZ',
                      'Multipart/Form-Data; boundary=XYZ']) {
        raw = await post(type, field('q', 'v'))
        ttrue(status(raw) == '200' && raw.contains('\r\nq=v\r\n'), `[${type}] must be accepted, got ${status(raw)}`)
    }
    for (let type of ['multipart/form-data; xboundary=XYZ', 'multipart/form-data; boundary=',
                      'multipart/form-data; boundary="XYZ']) {
        raw = await post(type, field('q', 'v'))
        ttrue(status(raw) == '400', `[${type}] must be refused, got ${status(raw)}`)
    }
}
