/*
    put-status.tst - PUT answers 201 for a new file, 204 for a replaced one, and refuses a directory with 409
 */
import { tget, ttrue } from '@embedthis/testme'
import { existsSync, readdirSync, readFileSync, rmSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const TMP = join(import.meta.dir, '..', 'web', 'tmp')
const FILE = join(TMP, 'zput-status.txt')
//  ME_GOAHEAD_PUT_DIR stages bodies in the server's working directory
const STAGING = join(import.meta.dir, '..')

function staged(): string[] {
    return readdirSync(STAGING).filter(name => name.startsWith('put-') && name.endsWith('.tmp'))
}

rmSync(FILE, { force: true })
try {
    let before = staged().length

    let response = await fetch(`${HTTP}/tmp/zput-status.txt`, { method: 'PUT', body: 'one' })
    ttrue(response.status == 201, `a PUT creating a file must get 201, got ${response.status}`)

    response = await fetch(`${HTTP}/tmp/zput-status.txt`, { method: 'PUT', body: 'two' })
    ttrue(response.status == 204, `a PUT replacing a file must get 204, got ${response.status}`)
    ttrue(existsSync(FILE) && readFileSync(FILE, 'utf8') == 'two', 'the replaced file must hold the second body')

    response = await fetch(`${HTTP}/tmp/`, { method: 'PUT', body: 'x' })
    ttrue(response.status == 409, `a PUT to a directory must get 409, got ${response.status}`)
    ttrue(staged().length == before, 'a refused PUT must not leave a staging file')
} finally {
    rmSync(FILE, { force: true })
}
