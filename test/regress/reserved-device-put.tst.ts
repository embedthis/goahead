/* PUT and DELETE refuse a Windows reserved device name */
import { tget, ttrue } from '@embedthis/testme'
import { existsSync, rmSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const TMP = join(import.meta.dir, '..', 'web', 'tmp')

if (process.platform == 'win32') {
    for (let uri of ['/tmp/COM1', '/tmp/nul.txt', '/tmp/zdev/aux']) {
        let put = await fetch(`${HTTP}${uri}`, { method: 'PUT', body: 'to a device' })
        ttrue(put.status == 404, `PUT ${uri} must be refused, got ${put.status}`)
        let del = await fetch(`${HTTP}${uri}`, { method: 'DELETE' })
        ttrue(del.status == 404, `DELETE ${uri} must be refused, got ${del.status}`)
    }
} else {
    try {
        let put = await fetch(`${HTTP}/tmp/COM1`, { method: 'PUT', body: 'ordinary file' })
        ttrue(put.status == 201, `PUT /tmp/COM1 must create an ordinary file, got ${put.status}`)
        ttrue(existsSync(join(TMP, 'COM1')), 'the PUT must create the file')
        let del = await fetch(`${HTTP}/tmp/COM1`, { method: 'DELETE' })
        ttrue(del.status == 204, `DELETE /tmp/COM1 must remove the ordinary file, got ${del.status}`)
        ttrue(!existsSync(join(TMP, 'COM1')), 'the DELETE must remove the file')
    } finally {
        rmSync(join(TMP, 'COM1'), { force: true })
    }
}

let response = await fetch(`${HTTP}/index.html`)
ttrue(response.status == 200, `the server must still serve after the device requests, got ${response.status}`)
