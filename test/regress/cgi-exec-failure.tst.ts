/*
    cgi-exec-failure.tst - A CGI that cannot be executed must not be served as 200 OK
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { chmodSync, unlinkSync, writeFileSync } from 'node:fs'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'

// CGI paths are relative to the server, one directory above the test.
const BROKEN_URL = '/cgi-bin/notaprogram'
const BROKEN = new URL('../cgi-bin/notaprogram', import.meta.url).pathname

if (thas('ME_GOAHEAD_CGI') && process.platform != 'win32') {
    let http: Http = new Http

    /*
        Make an executable with no valid program format so failure occurs in the child after access checks.
     */
    writeFileSync(BROKEN, 'this is not a program\n')
    chmodSync(BROKEN, 0o755)

    try {
        http.get(HTTP + BROKEN_URL)
        await http.wait()

        ttrue(http.status != 200, `a CGI that could not be executed must not be 200, got ${http.status}`)
        ttrue(http.status == 502, `a failed exec must be reported as 502, got ${http.status}`)
        http.close()

        http.reset()
        http.get(HTTP + '/cgi-bin/cgitest')
        await http.wait()
        ttrue(http.status == 200, 'a working CGI must still be served')
        http.close()

        http.reset()
        http.get(HTTP + '/index.html')
        await http.wait()
        ttrue(http.status == 200, 'the server must still serve after a failed CGI exec')
        http.close()
    } finally {
        try {
            unlinkSync(BROKEN)
        } catch {
        }
    }
}
