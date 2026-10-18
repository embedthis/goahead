/*
    isindex-argv-filter.tst - An ISINDEX search term is a word, not an option
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

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
    let match = raw.match(/HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

if (thas('ME_GOAHEAD_CGI')) {
    /*
        Encode option-like terms so they reach ISINDEX validation unchanged.
     */
    const REJECT = [
        '-rf',
        '%2Fswitch',        // Windows option form
        '%40file',          // response file
        '%2Bopt',           // plus option
        '%3Credirect',      // input redirection
        '%3Eoutfile',       // output redirection
        '%7Ccommand',       // pipe
        '.hidden',          // relative path
        '%09tab',           // leading control character
    ]
    for (let term of REJECT) {
        let raw = await rawGet('/cgi-bin/cgitest?' + term)
        ttrue(status(raw) == '400', `an ISINDEX term "${term}" must be refused, got ${status(raw)}`)
    }

    /*
        Check every term, including those after a valid one.
     */
    let raw = await rawGet('/cgi-bin/cgitest?good+%2Fswitch')
    ttrue(status(raw) == '400', 'every ISINDEX term must be checked, not just the first')

    raw = await rawGet('/cgi-bin/cgitest?alpha+beta9+9gamma')
    ttrue(status(raw) == '200', `ordinary search terms must be served, got ${status(raw)}`)
    ttrue(raw.contains('alpha') && raw.contains('beta9') && raw.contains('9gamma'),
          'the search terms must reach the program')

    ttrue(raw.contains('--'), 'the POSIX end-of-options marker must still be passed')

    raw = await rawGet('/cgi-bin/cgitest?opt=-rf')
    ttrue(status(raw) == '200', `a key=value query must not be treated as ISINDEX, got ${status(raw)}`)
}
