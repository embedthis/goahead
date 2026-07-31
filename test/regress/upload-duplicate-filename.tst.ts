/*
    upload-duplicate-filename.tst - A part may open only one upload file

    processUploadHeader ran the filename= branch once per matching pair, assigning a fresh open()
    result to wp->upfd without closing the descriptor already there. Only one descriptor is ever
    closed (at the end of the part, or at teardown), so one anonymous POST carrying N filename=
    params leaked N-1 descriptors, and freeUploadFile unlinked each prior temp file, leaving
    deleted-but-open inodes holding disk blocks. A few such requests exhausted the descriptor table
    and wedged the server. A part carrying two Content-Disposition header lines reached the same
    overwrite, because the per-header reset clears wp->clientFilename.

    The filename branch now refuses to open a second file while wp->upfd is open - that descriptor
    is per-part and is reset only when the part ends - so both spellings answer 400 and no descriptor
    is leaked.
*/

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)
const BOUNDARY = '----GoAheadRegress10014'

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

/*
    Build a multipart body from the supplied Content-Disposition header lines.
 */
function body(dispositions: string[]): string {
    let part = '--' + BOUNDARY + '\r\n'
    for (let disposition of dispositions) {
        part += disposition + '\r\n'
    }
    return part + '\r\n' + 'hello' + '\r\n' + '--' + BOUNDARY + '--\r\n'
}

if (thas('ME_GOAHEAD_UPLOAD')) {

    /*
        Baseline: a well formed part with a single filename= is still accepted.
     */
    let response = await rawPost(body(['Content-Disposition: form-data; name="myfile"; filename="one.dat"']))
    ttrue(status(response) == 200)
    ttrue(response.contains('CLIENT=one.dat'))
    ttrue(response.contains('SIZE=5'))

    /*
        Repeated filename= pairs in one Content-Disposition are a malformed part.
     */
    let repeated = 'Content-Disposition: form-data; name="myfile"'
    for (let i = 0; i < 300; i++) {
        repeated += '; filename="leak' + i + '.dat"'
    }
    response = await rawPost(body([repeated]))
    ttrue(status(response) == 400)

    /*
        Repeated Content-Disposition header lines reach the same open() and are refused too.
     */
    response = await rawPost(body(['Content-Disposition: form-data; name="myfile"; filename="one.dat"',
                                   'Content-Disposition: form-data; name="myfile"; filename="two.dat"']))
    ttrue(status(response) == 400)

    /*
        No descriptor is leaked. Before the fix these requests opened 300 files each and closed one,
        so the process ran out of descriptors and every later request failed. Uploads must still work
        after the barrage.
     */
    for (let i = 0; i < 20; i++) {
        response = await rawPost(body([repeated]))
        ttrue(status(response) == 400)
    }
    response = await rawPost(body(['Content-Disposition: form-data; name="myfile"; filename="after.dat"']))
    ttrue(status(response) == 200)
    ttrue(response.contains('CLIENT=after.dat'))
    ttrue(response.contains('SIZE=5'))

} else {
    tskip('Upload support not enabled')
}
