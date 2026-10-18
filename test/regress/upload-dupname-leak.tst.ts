/*
    upload-dupname-leak.tst - A repeated multipart field name must not orphan the prior part

    processContentData recorded each completed part in wp->files keyed by the client supplied field
    name. hashEnter frees only string values, so when a later part reused the field name the prior
    WebsUpload was dropped without unlinking its staging file or freeing its heap. The orphan was in
    neither cleanup path - no longer in wp->files, and wp->currentFile had already been cleared when
    the part completed - so websFreeUpload never released it. Every such request left one staging
    file on disk for good, and one unauthenticated request per leak is enough to exhaust the inodes
    of a small embedded filesystem.

    The fix releases any part already recorded under the field name before the entry is overwritten,
    so the staging directory returns to its baseline and retrieval keeps last-wins semantics.
*/

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { readdirSync } from 'node:fs'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)
const BOUNDARY = '----GoAheadRegress10248'
const ITERATIONS = 20

/*
    The upload filter stages each part as tmp-<random>.tmp in ME_GOAHEAD_UPLOAD_DIR, which is "tmp"
    relative to the server's working directory. A surviving part is renamed away by the uploadTest
    action, so anything left here after a request is a part that was never released.
 */
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

/*
    Build a multipart body from [name, filename, data] triples.
 */
function body(parts: string[][]): string {
    let text = ''
    for (let [name, filename, data] of parts) {
        text += '--' + BOUNDARY + '\r\n' +
                'Content-Disposition: form-data; name="' + name + '"; filename="' + filename + '"\r\n' +
                '\r\n' + data + '\r\n'
    }
    return text + '--' + BOUNDARY + '--\r\n'
}

if (thas('ME_GOAHEAD_UPLOAD')) {

    let baseline = staged()

    /*
        Two file parts sharing one field name. The second supersedes the first in wp->files, so the
        first must be released rather than orphaned.
     */
    for (let i = 0; i < ITERATIONS; i++) {
        let response = await rawPost(body([['dup', 'first.dat', 'AAAA'],
                                           ['dup', 'second.dat', 'CCCCCCCC']]))
        ttrue(status(response) == 200)
        ttrue(response.contains('CLIENT=second.dat'))
        ttrue(response.contains('SIZE=8'))
        ttrue(!response.contains('CLIENT=first.dat'))
    }

    /*
        Before the fix this count grew by one per request and nothing ever reclaimed it, on disk or
        at teardown.
     */
    ttrue(staged() == baseline)

    /*
        Distinct field names are unaffected: both parts are recorded and both are cleaned up.
     */
    let response = await rawPost(body([['one', 'one.dat', 'AAAA'],
                                       ['two', 'two.dat', 'BBBBBB']]))
    ttrue(status(response) == 200)
    ttrue(response.contains('CLIENT=one.dat'))
    ttrue(response.contains('CLIENT=two.dat'))
    ttrue(staged() == baseline)

    /*
        A single part still uploads correctly after the barrage.
     */
    response = await rawPost(body([['myfile', 'after.dat', 'hello']]))
    ttrue(status(response) == 200)
    ttrue(response.contains('CLIENT=after.dat'))
    ttrue(response.contains('SIZE=5'))
    ttrue(staged() == baseline)

} else {
    tskip('Upload support not enabled')
}
