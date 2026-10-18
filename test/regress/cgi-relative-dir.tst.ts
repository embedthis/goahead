/*
    cgi-relative-dir.tst - A CGI route with a relative dir= finds its program
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, symlinkSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

const TEST = fileURLToPath(new URL('..', import.meta.url))

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

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

if (thas('ME_GOAHEAD_CGI') && process.platform != 'win32') {
    rmSync(TEST + 'reldir', { recursive: true, force: true })
    mkdirSync(TEST + 'reldir')
    symlinkSync('../cgi-bin', TEST + 'reldir/relative')
    try {
        let raw = await get('/relative/cgitest')
        ttrue(status(raw) == '200', `a program under a relative dir= must run, got ${status(raw)}`)

        let match = raw.match(/SCRIPT_FILENAME=([^<\r\n]*)/)
        let filename = match ? match[1] : ''
        ttrue(filename.startsWith('/') && filename.endsWith('/reldir/relative/cgitest'),
              `SCRIPT_FILENAME must be the program's absolute path, got [${filename}]`)
    } finally {
        rmSync(TEST + 'reldir', { recursive: true, force: true })
    }
}
