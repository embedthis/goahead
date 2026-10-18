/* Only regular files may be opened and served */

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
    ttrue(status(response) == 404, `${uri} must be refused as not found, got ${status(response)}`)
}

/*
    Reject device stream paths before opening; test the device check separately.
 */
for (let uri of ['/CON::$DATA', '/con:x', '/prn:', '/nul.txt:s', '/tmp/aux::$DATA']) {
    response = await rawGet(uri)
    ttrue(status(response) == 400, `${uri} must be refused as non-canonical, got ${status(response)}`)
}

/*
    The server is still serving after every one of the above -- the event loop was never blocked.
 */
response = await rawGet('/index.html')
ttrue(status(response) == 200)
