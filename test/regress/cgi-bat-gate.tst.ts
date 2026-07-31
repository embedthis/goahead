/*
    cgi-bat-gate.tst - The Windows CGI extension gate must never launch a .bat target.

    Modern Windows kernel32 routes a .bat/.cmd target through cmd.exe transparently, whatever
    lpApplicationName says, so a CGI request for a .bat delivered its query string to a shell
    (the "BatBadBut" trap) - unauthenticated RCE. The gate now admits only a true ".exe" suffix.

    Encodes the reporter's exact PoC as a permanent regression assertion. The extension gate is
    Windows-only - POSIX runs execve on any executable file and has no shell to reach - so the 404
    is asserted on Windows, while every platform asserts that no shell interpreted the query. The
    near-miss name "cgitest.batch.exe" - which contains the substring ".bat" but ends in ".exe" -
    must still launch, so the gate is a suffix test and not a substring test.
 */

import { Config, Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const httpUri = new Uri(HTTP)

async function rawGet(uri: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write('GET ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n\r\n')
    let response = new ByteArray(65536)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

const isWindows = Config.OS == 'windows'

if (thas('ME_GOAHEAD_CGI')) {

    /*
        Criterion #4: a plain .bat request is refused by the Windows extension gate.
     */
    let response = await rawGet('/cgi-bin/probe.bat?x')
    if (isWindows) {
        ttrue(status(response) == 404, 'a .bat CGI target must not be launched')
    }
    ttrue(!response.includes('bat-marker-launched'), 'a .bat CGI target must not execute')

    /*
        Criterion #5: the reporter's exact PoC - "x & whoami & rem" URL-encoded. A shell launch
        would echo the bat marker and the current user name into the body.
     */
    response = await rawGet('/cgi-bin/probe.bat?x+%26+whoami+%26+rem')
    if (isWindows) {
        ttrue(status(response) == 404, 'the reporter PoC must be refused')
    }
    ttrue(!response.includes('bat-marker-launched'), 'the reporter PoC must not reach cmd.exe')
    ttrue(!response.includes('uid='), 'the reporter PoC must not report a POSIX identity')

    /*
        Criterion #6: the gate is a suffix match. "cgitest.batch.exe" contains ".bat" but ends
        ".exe", so it must launch normally. cgitest with no switches emits its environment.
     */
    response = await rawGet('/cgi-bin/cgitest.batch.exe')
    ttrue(status(response) == 200, 'an .exe whose name contains ".bat" must still launch')
    ttrue(response.includes('CONTENT_LENGTH='), 'the near-miss .exe must run the CGI program')

} else {
    tskip('CGI not enabled')
}
