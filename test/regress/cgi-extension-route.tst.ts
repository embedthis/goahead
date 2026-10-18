/* On a CGI route restricted by extensions=, the program run must have a permitted extension */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { copyFileSync, existsSync, linkSync, mkdirSync, rmSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const BIN = join(import.meta.dir, '..', '..', 'build', 'bin')
const DIR = join(import.meta.dir, '..', 'extcgi')

/*
    Link fixtures when possible to avoid macOS first-run executable checks under load.
 */
function install(from: string, to: string) {
    try {
        linkSync(from, to)
    } catch {
        copyFileSync(from, to)
    }
}

async function get(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: localhost\r\nSWITCHES: -e\r\n\r\n')
    let buf = new ByteArray(65536, true)
    try {
        while ((await s.read(buf, -1)) != null) {}
    } catch {
    }
    s.close()
    return buf.toString()
}

function status(raw: string): number {
    let m = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return m ? parseInt(m[1]) : 0
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not enabled')
} else if (process.platform == 'win32') {
    tskip('Windows runs only .exe CGI programs')
} else {
    mkdirSync(DIR, { recursive: true })
    try {
        for (let name of ['ok.cgi', 'helper']) {
            install(join(BIN, 'cgitest'), join(DIR, name))
        }
        for (let lib of ['libgoahead.dylib', 'libgoahead.so']) {
            if (existsSync(join(BIN, lib))) {
                install(join(BIN, lib), join(DIR, lib))
            }
        }
        let raw = await get('/extcgi/helper/x.cgi')
        ttrue(status(raw) == 404, `a program without the .cgi extension must not run, got ${status(raw)}`)
        ttrue(!raw.contains('PATH_INFO'), 'none of the program output may be sent')

        raw = await get('/extcgi/ok.cgi/x.cgi')
        ttrue(status(raw) == 200, `ok.cgi must run, got ${status(raw)}`)
        ttrue(raw.contains('PATH_INFO=/x.cgi'), 'ok.cgi must receive PATH_INFO /x.cgi')

        raw = await get('/extcgi/ok.cgi')
        ttrue(status(raw) == 200, `ok.cgi must run without PATH_INFO, got ${status(raw)}`)
    } finally {
        rmSync(DIR, { recursive: true, force: true })
    }
}
