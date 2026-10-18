/* A multipart delimiter is CRLF "--" boundary followed by "--" or padding and CRLF */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { rmSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function upload(body: string, split: number = -1): Promise<string> {
    let head = `POST /action/uploadTest HTTP/1.0\r\nHost: ${ADDR}\r\n` +
        `Content-Type: multipart/form-data; boundary=XYZ\r\nContent-Length: ${body.length}\r\n\r\n`
    let s = new Socket
    s.connect(ADDR)
    if (split < 0) {
        await s.write(head + body)
    } else {
        await s.write(head + body.slice(0, split))
        await new Promise(r => setTimeout(r, 300))
        await s.write(body.slice(split))
    }
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): number {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Uploads are not in this build')
} else {
    try {
        //  The boundary inside file data, with no CRLF before it, is data
        let data = 'innocent--XYZ\r\nContent-Disposition: form-data; name="injected"\r\n\r\nyes'
        let raw = await upload('--XYZ\r\nContent-Disposition: form-data; name="f"; filename="zanchor.txt"\r\n\r\n' +
            data + '\r\n--XYZ--\r\n')
        ttrue(status(raw) == 200, `an unanchored boundary must be kept as data, got ${status(raw)}`)
        ttrue(raw.includes(`SIZE=${data.length}\r\n`), 'the file must hold all the data before the real delimiter')
        ttrue(!raw.includes('injected='), 'the data must not define a field')

        //  A line that only begins with the delimiter is data
        raw = await upload('--XYZ\r\nContent-Disposition: form-data; name="a"\r\n\r\none\r\n' +
            '--XYZW\r\nContent-Disposition: form-data; name="b"\r\n\r\ntwo\r\n--XYZ--\r\n')
        ttrue(status(raw) == 200, `a boundary prefix line must be kept as data, got ${status(raw)}`)
        ttrue(!raw.includes('\r\nb=two'), 'the prefix line must not open a part')
        ttrue(raw.includes('a=one\r\n--XYZW\r\n'), 'the prefix line must stay in the value')

        //  Transport padding after a delimiter is allowed
        raw = await upload('--XYZ  \r\nContent-Disposition: form-data; name="a"\r\n\r\none\r\n' +
            '--XYZ \t\r\nContent-Disposition: form-data; name="b"\r\n\r\ntwo\r\n--XYZ--\r\n')
        ttrue(status(raw) == 200, `a padded delimiter must be accepted, got ${status(raw)}`)
        ttrue(raw.includes('a=one\r\n') && raw.includes('b=two\r\n'), 'both parts must be delivered')


        let body = '--XYZ\r\nContent-Disposition: form-data; name="a"\r\n\r\none\r\n--XYZ\r\n' +
            'Content-Disposition: form-data; name="f"; filename="zanchor.txt"\r\n\r\nfiledata\r\n--XYZ--\r\n'
        for (let at of [body.indexOf('one') + 4, body.indexOf('filedata') + 11]) {
            raw = await upload(body, at)
            ttrue(status(raw) == 200, `a delimiter split at ${at} must be found, got ${status(raw)}`)
            ttrue(raw.includes('a=one\r\n') && raw.includes('SIZE=8\r\n'), `both parts split at ${at} must be whole`)
        }

        //  A delimiter line with anything else after the boundary is refused
        raw = await upload('--XYZjunk\r\nContent-Disposition: form-data; name="a"\r\n\r\none\r\n--XYZ--\r\n')
        ttrue(status(raw) == 400, `a malformed first delimiter must be refused, got ${status(raw)}`)
    } finally {
        rmSync(join(import.meta.dir, '..', 'web', 'tmp', 'zanchor.txt'), { force: true })
    }
}
