/* Client parameters cannot set the server's upload variables */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { rmSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const BOUNDARY = 'XYZ'

function part(name: string, value: string): string {
    return `--${BOUNDARY}\r\nContent-Disposition: form-data; name="${name}"\r\n\r\n${value}\r\n`
}

async function upload(fieldName: string, value: string, query: string = ''): Promise<string> {
    let body = `--${BOUNDARY}\r\nContent-Disposition: form-data; name="f"; filename="zshadow.txt"\r\n` +
        `Content-Type: text/plain\r\n\r\nFILEDATA\r\n` +
        part(fieldName, value) + part('FILE_SIZE_f', '1') + part('UPLOAD_DIR', '/') +
        part('FILE_FILENAME_g', '/etc/passwd') + `--${BOUNDARY}--\r\n`
    let response = await fetch(`${HTTP}/action/uploadTest${query}`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        body,
    })
    ttrue(response.status == 200, `the upload must be accepted, got ${response.status}`)
    return await response.text()
}

function check(text: string, why: string, literal: string = '') {
    let staged = (text.match(/FILENAME=(.*)\r\n/) || [])[1]
    ttrue(!!staged, `${why}: the upload must list the staged file`)
    ttrue(text.includes(`FILE_FILENAME_f=${staged}\r\n`), `${why}: FILE_FILENAME_f must name the staged file`)
    ttrue(text.includes('FILE_SIZE_f=8\r\n'), `${why}: FILE_SIZE_f must be the real size`)
    ttrue(!text.includes('UPLOAD_DIR=/\r\n'), `${why}: UPLOAD_DIR must be the server's directory`)
    ttrue(!text.includes('FILE_FILENAME_g='), `${why}: a client name must not create an upload variable`)
    if (literal) {
        ttrue(text.includes(`${literal}=/etc/passwd\r\n`), `${why}: the name must be stored as sent`)
        text = text.replace(`${literal}=/etc/passwd\r\n`, '')
    }
    ttrue(!text.includes('/etc/passwd'), `${why}: no other variable may hold the client's path`)
}

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Uploads are not in this build')
} else {
    try {
        check(await upload('FILE_FILENAME_f', '/etc/passwd'), 'a text part')
        check(await upload('FILE%5FFILENAME_f', '/etc/passwd'), 'an encoded text part', 'FILE%5FFILENAME_f')
        check(await upload('note', 'ordinary', '?FILE_FILENAME_f=/etc/passwd&FILE_FILENAME_g=/etc/passwd'), 'the query')
    } finally {
        rmSync(join(import.meta.dir, '..', 'web', 'tmp', 'zshadow.txt'), { force: true })
    }
}
