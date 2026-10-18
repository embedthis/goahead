/*
    hidden-files.tst - Hidden and backup files in the documents are not served
 */
import { tget, ttrue } from '@embedthis/testme'
import { existsSync, mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const WEB = join(import.meta.dir, '..', 'web')

const REFUSED = ['zhidden/.env', 'zhidden/.git/config', 'zhidden/.cache/page.txt', 'zhidden/notes.txt~',
    'zhidden/page.bak', 'zhidden/page.BAK', 'zhidden/page.orig', 'zhidden/page.swp', 'zhidden/page.tmp',
    'zhidden/sub/.well-known/page.txt', 'zhidden/site.bak/index.html', 'zhidden/old~/index.html',
    'zhidden/cache.tmp/page.txt']
const SERVED = ['.well-known/zhidden-page.txt', 'zhidden/a.b.txt', 'zhidden/page.bak.txt', 'zhidden/dots..txt',
    'zhidden/v1.bak.d/page.txt']

function write(path: string) {
    let file = join(WEB, path)
    mkdirSync(join(file, '..'), { recursive: true })
    writeFileSync(file, 'content of ' + path)
}

const hadWellKnown = existsSync(join(WEB, '.well-known'))
try {
    for (let path of [...REFUSED, ...SERVED]) {
        write(path)
    }
    for (let path of REFUSED) {
        let response = await fetch(`${HTTP}/${path}`)
        let text = await response.text()
        ttrue(response.status == 404, `GET /${path} must be refused, got ${response.status}`)
        ttrue(!text.includes('content of'), `GET /${path} must not return the file`)
    }
    for (let path of SERVED) {
        let response = await fetch(`${HTTP}/${path}`)
        ttrue(response.status == 200 && (await response.text()) == 'content of ' + path,
              `GET /${path} must be served, got ${response.status}`)
    }

    let dir = await fetch(`${HTTP}/zhidden/site.bak`, { redirect: 'manual' })
    ttrue(dir.status == 404, `GET of a backup directory must be refused, not redirected, got ${dir.status}`)

    let put = await fetch(`${HTTP}/tmp/.zhidden-put`, { method: 'PUT', body: 'planted' })
    ttrue(put.status == 404, `PUT of a hidden file must be refused, got ${put.status}`)
    ttrue(!existsSync(join(WEB, 'tmp', '.zhidden-put')), 'the refused PUT must not create the file')

    write('tmp/.zhidden-delete')
    let del = await fetch(`${HTTP}/tmp/.zhidden-delete`, { method: 'DELETE' })
    ttrue(del.status == 404, `DELETE of a hidden file must be refused, got ${del.status}`)
    ttrue(existsSync(join(WEB, 'tmp', '.zhidden-delete')), 'the refused DELETE must not remove the file')
} finally {
    rmSync(join(WEB, 'zhidden'), { recursive: true, force: true })
    rmSync(join(WEB, hadWellKnown ? '.well-known/zhidden-page.txt' : '.well-known'), { recursive: true, force: true })
    rmSync(join(WEB, 'tmp', '.zhidden-put'), { force: true })
    rmSync(join(WEB, 'tmp', '.zhidden-delete'), { force: true })
}
