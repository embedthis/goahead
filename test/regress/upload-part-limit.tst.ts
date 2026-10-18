/* Parts and staged files per upload request are limited */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { readdirSync } from 'node:fs'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const STAGING = '../tmp'
const UPLOAD_FILES = 16             // ME_GOAHEAD_LIMIT_UPLOAD_FILES
const PARAMS = 512                  // ME_GOAHEAD_LIMIT_PARAMS

async function post(body: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST /action/uploadTest HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n' +
                  'Content-Type: multipart/form-data; boundary=XYZ\r\nContent-Length: ' + body.length + '\r\n\r\n' +
                  body)
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

function staged(): number {
    return readdirSync(STAGING).filter((name) => name.startsWith('tmp-') && name.endsWith('.tmp')).length
}

function files(count: number): string {
    let body = ''
    for (let i = 0; i < count; i++) {
        body += '--XYZ\r\nContent-Disposition: form-data; name="k' + i + '"; filename="partlimit' + i + '.txt"\r\n\r\nx\r\n'
    }
    return body + '--XYZ--\r\n'
}

function fields(count: number): string {
    let body = ''
    for (let i = 0; i < count; i++) {
        body += '--XYZ\r\nContent-Disposition: form-data; name="f' + i + '"\r\n\r\nx\r\n'
    }
    return body + '--XYZ--\r\n'
}

if (thas('ME_GOAHEAD_UPLOAD')) {
    let baseline = staged()

    let raw = await post(files(UPLOAD_FILES + 1))
    ttrue(status(raw) == '413', `${UPLOAD_FILES + 1} files must be refused, got ${status(raw)}`)
    ttrue(staged() == baseline, 'the files staged before the refusal must be removed')

    raw = await post(files(UPLOAD_FILES))
    ttrue(status(raw) == '200', `${UPLOAD_FILES} files must be accepted, got ${status(raw)}`)
    ttrue(raw.split('FILE: ').length - 1 == UPLOAD_FILES, 'every file at the limit must be received')

    raw = await post(fields(PARAMS + 1))
    ttrue(status(raw) == '413', `${PARAMS + 1} parts must be refused, got ${status(raw)}`)

    raw = await post(fields(PARAMS))
    ttrue(status(raw) == '200', `${PARAMS} parts must be accepted, got ${status(raw)}`)
}
