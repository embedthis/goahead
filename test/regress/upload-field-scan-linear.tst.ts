/* A large multipart text field is scanned for its delimiter once, not per read */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'
import { rmSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const BOUNDARY = '-'.repeat(69) + 'Q'
const VALUE = '-'.repeat(1024 * 1024)

async function post(disposition: string): Promise<{ status: number, text: string, elapsed: number }> {
    let body = `--${BOUNDARY}\r\nContent-Disposition: form-data; ${disposition}\r\n\r\n${VALUE}\r\n--${BOUNDARY}--\r\n`
    let started = Date.now()
    let response = await fetch(`${HTTP}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        body,
    })
    let text = await response.text()
    return { status: response.status, text, elapsed: Date.now() - started }
}

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Uploads are not in this build')
} else {
    try {
        let control = await post('name="t"; filename="zscan-linear.bin"')
        ttrue(control.status == 200, `the file part control must be accepted, got ${control.status}`)
        ttrue(control.text.includes(`SIZE=${VALUE.length}`), 'the file part must be stored at its full length')

        let field = await post('name="t"')
        ttrue(field.status == 200, `the text field must be accepted, got ${field.status}`)
        ttrue(field.text.includes(`t=${VALUE}\r\n`), 'the text field must be delivered at its full length')
        let limit = 5 * Math.max(control.elapsed, 100)
        ttrue(field.elapsed <= limit,
            `the text field took ${field.elapsed} ms against ${control.elapsed} ms for the file part`)
    } finally {
        rmSync(join(import.meta.dir, '..', 'web', 'tmp', 'zscan-linear.bin'), { force: true })
    }
}
