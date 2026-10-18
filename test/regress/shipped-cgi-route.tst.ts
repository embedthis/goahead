/*
    shipped-cgi-route.tst - The CGI route shipped as an example is the one the suite runs
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

const TEST = fileURLToPath(new URL('..', import.meta.url))

function normalize(line: string): string {
    return line.trim().split(/\s+/).join(' ')
}

async function get(target: string): Promise<string> {
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

let shipped = readFileSync(TEST + '../src/route.txt', 'utf8').split('\n')
    .filter(line => /^#\s*route\s+uri=\/cgi-bin\b/.test(line))
    .map(line => normalize(line.replace(/^#\s*/, '')))
ttrue(shipped.length == 1, `src/route.txt must carry one CGI example route, found ${shipped.length}`)

let tested = readFileSync(TEST + 'route.txt', 'utf8').split('\n').map(normalize)
ttrue(tested.includes(shipped[0]), `the shipped CGI example "${shipped[0]}" must be the route the suite tests`)

if (thas('ME_GOAHEAD_CGI')) {
    let raw = await get('/cgi-bin/cgitest')
    ttrue(raw.contains('200 OK'), 'the example route must run a program')
}
