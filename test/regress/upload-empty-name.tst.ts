/* A multipart part with an empty name must be refused */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { readdirSync } from 'node:fs'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)
const BOUNDARY = '----GoAheadRegressEmptyName'

//  Upload staging directory, relative to the test directory (see upload-dupname-leak.tst.ts)
const STAGING = '../tmp'

async function rawPost(body: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write('POST /action/uploadTest HTTP/1.0\r\n' +
                  'Host: localhost\r\n' +
                  'Content-Type: multipart/form-data; boundary=' + BOUNDARY + '\r\n' +
                  'Content-Length: ' + body.length + '\r\n' +
                  'Connection: close\r\n\r\n' + body)
    let response = new ByteArray(65536)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

function staged(): number {
    return readdirSync(STAGING).filter((name) => name.startsWith('tmp-') && name.endsWith('.tmp')).length
}

function part(disposition: string, data: string): string {
    return '--' + BOUNDARY + '\r\n' + 'Content-Disposition: form-data; ' + disposition + '\r\n\r\n' + data + '\r\n'
}

function close(): string {
    return '--' + BOUNDARY + '--\r\n'
}

if (thas('ME_GOAHEAD_UPLOAD')) {

    let baseline = staged()

    //  Text part with an empty name
    let response = await rawPost(part('name=""', 'value') + close())
    ttrue(status(response) == 400)

    //  File part with an empty name: refused before the part is staged
    response = await rawPost(part('name=""; filename="empty.dat"', 'AAAA') + close())
    ttrue(status(response) == 400)
    ttrue(staged() == baseline)


    response = await rawPost(part('name="good"; filename="good.dat"', 'BBBB') +
                             part('name=""', 'value') + close())
    ttrue(status(response) == 400)
    ttrue(staged() == baseline)

    //  The server is still running and uploads still work
    response = await rawPost(part('name="myfile"; filename="after.dat"', 'hello') + close())
    ttrue(status(response) == 200)
    ttrue(response.contains('CLIENT=after.dat'))
    ttrue(response.contains('SIZE=5'))
    ttrue(staged() == baseline)

} else {
    tskip('Upload support not enabled')
}
