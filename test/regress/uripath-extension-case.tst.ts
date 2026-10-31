/*
    Preserve extension case in file paths and CGI meta-variables while matching extensions case-insensitively.
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const DIR = join(import.meta.dir, '..', 'web', 'zextcase')

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

function env(raw: string, name: string): string {
    let match = raw.match(new RegExp(name + '=([^<\\r\\n]*)'))
    return match ? match[1] : ''
}

/*
    Use an uppercase extension to detect path mutation on case-sensitive file systems.
 */
mkdirSync(DIR, { recursive: true })
writeFileSync(join(DIR, 'Page.TXT'), 'upper-extension-document')
try {
    let raw = await get('/zextcase/Page.TXT')
    ttrue(status(raw) == '200', `the document must be served under its own spelling, got ${status(raw)}`)
    ttrue(raw.contains('upper-extension-document'), 'the document body must be served')
} finally {
    rmSync(DIR, { recursive: true, force: true })
}

/*
    The CGI meta-variables derived from the canonical path carry the extension as requested.
 */
if (thas('ME_GOAHEAD_CGI')) {
    let raw = await get('/cgi-bin/cgitest/Sub.Dir/Extra.HTML?x=1')
    ttrue(status(raw) == '200', `cgitest must run, got ${status(raw)}`)
    ttrue(env(raw, 'PATH_INFO') == '/Sub.Dir/Extra.HTML',
          `PATH_INFO must keep the request's extension case, got [${env(raw, 'PATH_INFO')}]`)
    ttrue(env(raw, 'PATH_TRANSLATED').replace(/\\/g, '/').endsWith('/web/Sub.Dir/Extra.HTML'),
          `PATH_TRANSLATED must keep the request's extension case, got [${env(raw, 'PATH_TRANSLATED')}]`)
    ttrue(env(raw, 'REQUEST_URI') == '/cgi-bin/cgitest/Sub.Dir/Extra.HTML?x=1',
          `REQUEST_URI is the target as sent, got [${env(raw, 'REQUEST_URI')}]`)

    //  Extension selection itself stays case-insensitive: a route listing cgi still runs prog.CGI
    raw = await get('/cgi-bin/cgitest/Extra.HTML')
    ttrue(env(raw, 'PATH_INFO') == '/Extra.HTML', `PATH_INFO must be as requested, got [${env(raw, 'PATH_INFO')}]`)
}
