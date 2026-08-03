/*
    file-type-gate.tst - Only regular files may be opened and served

    The static path used to perform one type check (websPageIsDirectory) and then hand the path to a
    blocking open(). open() on a FIFO with no writer blocks in the kernel, and GoAhead has a single
    event loop, so the call stalled the whole server: no crash, no log entry and no timeout, because
    the timeout machinery is driven by the loop that is now stuck. On Windows the reserved device
    names resolve in any directory, so an unauthenticated GET /CON reached the same open().

    websOpenFile now opens with O_NONBLOCK, requires S_ISREG of the descriptor, and on Windows
    refuses a reserved device name before the open. Every non-regular target answers 404 and the
    server stays responsive.

    The FIFO fixture web/fifo.txt is created by prep.sh on non-Windows hosts.
*/

import { Socket, ByteArray, Config, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)

async function rawGet(uri: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write('GET ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n\r\n')
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
    Baseline: a regular file is still served.
 */
let response = await rawGet('/index.html')
ttrue(status(response) == 200)

/*
    A FIFO in the document root must be refused rather than opened. Before the fix this request never
    returned and the server never answered another client. The fixture is POSIX only.
 */
if (Config.OS != 'windows' && Config.OS != 'win32') {
    response = await rawGet('/fifo.txt')
    ttrue(status(response) == 404)
}

/*
    The Windows reserved device names must be refused in any directory, in any case and with or
    without an extension. On POSIX these are ordinary missing files, so 404 is the correct answer on
    every platform. A trailing dot or space is rejected earlier as a non-canonical path, so those
    spellings are not exercised here.
 */
for (let uri of ['/CON', '/con.txt', '/nul', '/AUX', '/COM1', '/lpt1.html', '/tmp/con']) {
    response = await rawGet(uri)
    ttrue(status(response) == 404)
}

/*
    The server is still serving after every one of the above -- the event loop was never blocked.
 */
response = await rawGet('/index.html')
ttrue(status(response) == 200)
