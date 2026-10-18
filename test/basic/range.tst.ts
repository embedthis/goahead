/*
    range.tst - Range requests are answered with the whole resource
 */

import { tget, ttrue } from '@embedthis/testme'
import { rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const NAME = 'range-fixture.txt'
const PATH = join(import.meta.dir, '..', 'web', NAME)

let body = ''
for (let i = 0; body.length < 1024; i++) {
    body += String(i).padStart(4, '0') + '\n'
}

const RANGES = ['bytes=0-4', 'bytes=-5', 'bytes=1000-', 'bytes=0-5,25-30,-5']

writeFileSync(PATH, body)
try {
    for (let range of RANGES) {
        let response = await fetch(`${HTTP}/${NAME}`, { headers: { Range: range } })
        let text = await response.text()
        ttrue(response.status == 200, `Range ${range} must be answered 200, got ${response.status}`)
        ttrue(text == body, `Range ${range} must return the whole file, got ${text.length} of ${body.length} bytes`)
        ttrue(!response.headers.has('content-range'), `Range ${range} must not return a Content-Range`)
        ttrue(!response.headers.has('accept-ranges'), `Range ${range} must not advertise Accept-Ranges`)
    }
} finally {
    rmSync(PATH, { force: true })
}
