/* A long multipart line without a line feed is scanned once as it arrives, not once per read */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const BOUNDARY = '-'.repeat(69) + 'L'

async function post(size: number): Promise<{ status: number, elapsed: number }> {
    //  A part header line that never ends: the body stops before any line feed arrives
    let body = `--${BOUNDARY}\r\nContent-Disposition: form-data; name="` + 'n'.repeat(size)
    let started = Date.now()
    let response = await fetch(`${HTTP}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}` },
        body,
    })
    await response.text()
    return { status: response.status, elapsed: Date.now() - started }
}

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Uploads are not in this build')
} else {
    let small = await post(2 * 1024 * 1024)
    ttrue(small.status == 400, `an unterminated part header is refused, got ${small.status}`)
    let large = await post(8 * 1024 * 1024)
    ttrue(large.status == 400, `an unterminated part header is refused, got ${large.status}`)
    //  Four times the bytes: linear scanning costs about four times, quadratic sixteen
    let limit = 8 * Math.max(small.elapsed, 50)
    ttrue(large.elapsed <= limit, `8 MB took ${large.elapsed} ms against ${small.elapsed} ms for 2 MB`)
}
