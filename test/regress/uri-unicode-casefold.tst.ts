/*
    A spelling that a case-insensitive file system folds to ASCII never reaches protected content, template source
    or a backup file
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const FOLDS = process.platform == 'darwin' || process.platform == 'win32'
const DIR = join(import.meta.dir, '..', 'web', 'zcasefold')

async function get(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: localhost\r\n\r\n')
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

async function refused(target: string, secret: string, what: string) {
    let raw = await get(target)
    ttrue(!(status(raw) == 200 && raw.contains(secret)), `${target} must not serve ${what}, got ${status(raw)}`)
    if (FOLDS) {
        ttrue(status(raw) == 400, `${target} must be refused on a case-insensitive file system, got ${status(raw)}`)
    }
}

mkdirSync(DIR, { recursive: true })
writeFileSync(join(DIR, 'page.bak'), 'backup-secret')
writeFileSync(join(DIR, 'page.swp'), 'swap-secret')
try {
    //  Controls: the ASCII spellings are rendered, challenged or refused
    if (thas('ME_GOAHEAD_JAVASCRIPT')) {
        let raw = await get('/test.jst')
        ttrue(status(raw) == 200 && !raw.contains('<%'), 'the template must be rendered')
    }
    if (thas('ME_GOAHEAD_AUTH')) {
        ttrue(status(await get('/auth/basic/basic.html')) == 401, 'the protected page must be challenged')
    }
    ttrue(status(await get('/zcasefold/page.bak')) == 404, 'the backup file must be refused')

    //  Template source: U+017F long s, U+FB06 "st" ligature
    await refused('/test.j%C5%BFt', '<%', 'template source')
    await refused('/te%EF%AC%86.jst', '<%', 'template source')

    //  Authentication: U+017F long s
    if (thas('ME_GOAHEAD_AUTH')) {
        await refused('/auth/ba%C5%BFic/basic.html', 'Welcome to Basic', 'the protected page')
    }

    //  Hidden and backup files: U+212A Kelvin sign, U+017F long s
    await refused('/zcasefold/page.ba%E2%84%AA', 'backup-secret', 'the backup file')
    await refused('/zcasefold/page.%C5%BFwp', 'swap-secret', 'the swap file')

    //  U+0131 dotless i and U+0130 dotted capital I, which NTFS upcases to I
    if (thas('ME_GOAHEAD_AUTH')) {
        await refused('/auth/bas%C4%B1c/basic.html', 'Welcome to Basic', 'the protected page')
        await refused('/auth/bas%C4%B0c/basic.html', 'Welcome to Basic', 'the protected page')
    }
} finally {
    rmSync(DIR, { recursive: true, force: true })
}
