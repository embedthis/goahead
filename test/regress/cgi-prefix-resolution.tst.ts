/*
    cgi-prefix-resolution.tst - A CGI program is found under the route that selected it
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, symlinkSync } from 'node:fs'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

const TEST = new URL('..', import.meta.url).pathname

async function rawGet(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
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

/*
    Link the directory so cgitest still finds its adjacent library.
 */
function makeFixtures() {
    removeFixtures()
    mkdirSync(TEST + 'deep')
    symlinkSync('../cgi-bin', TEST + 'deep/cgi-bin')
    symlinkSync('cgi-bin', TEST + 'sib.bak')
}

function removeFixtures() {
    rmSync(TEST + 'deep', { recursive: true, force: true })
    rmSync(TEST + 'sib.bak', { force: true })
}

if (thas('ME_GOAHEAD_CGI') && process.platform != 'win32') {
    makeFixtures()
    try {
        let raw = await rawGet('/deep/cgi-bin/cgitest')
        ttrue(status(raw) == '200', `a program under a multi-segment prefix must run, got ${status(raw)}`)
        ttrue(raw.contains('SCRIPT_NAME=/deep/cgi-bin/cgitest'),
              'SCRIPT_NAME must name the program under the route prefix')

        raw = await rawGet('/deep/cgi-bin/cgitest/extra/path')
        ttrue(status(raw) == '200', `extra path must not change which program runs, got ${status(raw)}`)
        ttrue(raw.contains('PATH_INFO=/extra/path'), 'the path after the program must be PATH_INFO')

        /*
            A literal prefix may match a sibling, but must not execute from it.
         */
        raw = await rawGet('/sib.bak/cgitest')
        ttrue(status(raw) == '404', `a program in a sibling of the route directory must not run, got ${status(raw)}`)
        ttrue(!raw.contains('SCRIPT_NAME='), 'no CGI environment may be built for the sibling')

        raw = await rawGet('/deep/cgi-bin/')
        ttrue(status(raw) == '404', `a prefix with no program must be not found, got ${status(raw)}`)
    } finally {
        removeFixtures()
    }

    let raw = await rawGet('/cgi-bin/cgitest')
    ttrue(status(raw) == '200', `the ordinary CGI route must still run, got ${status(raw)}`)
    ttrue(raw.contains('SCRIPT_NAME=/cgi-bin/cgitest'), 'the ordinary SCRIPT_NAME must be unchanged')
}
