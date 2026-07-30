/*
    upload.tst - Stress test uploads — scales payload size by test depth and verifies
    the server stored the exact bytes that were transmitted.
 */

import { Http, Path } from '@embedthis/ejscript'
import { tdepth, tget, thas, tskip, ttrue } from '@embedthis/testme'

function hashcode(value: any): number {
    const str = String(value)
    let hash = 0
    for (let i = 0; i < str.length; i++) {
        hash = ((hash << 5) - hash) + str.charCodeAt(i)
        hash = hash & hash
    }
    return Math.abs(hash)
}

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const TESTFILE = 'upload-' + hashcode(process.pid) + '.tdat'

if (thas('ME_GOAHEAD_UPLOAD')) {
    let http: Http = new Http

    /* Depths:    0  1  2  3   4   5   6    7    8    9    */
    const sizes = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512]

    //  Build 16-byte line pattern and replicate to (sizes[depth] * 1024) lines.
    const line = new Uint8Array(16)
    for (let j = 0; j < 15; j++) {
        line[j] = 'A'.charCodeAt(0) + (j % 26)
    }
    line[15] = '\n'.charCodeAt(0)
    const lineCount = sizes[tdepth()] * 1024
    const payload = new Uint8Array(line.length * lineCount)
    for (let i = 0; i < lineCount; i++) {
        payload.set(line, i * line.length)
    }
    await Bun.write(TESTFILE, payload)

    try {
        const expectedSize = new Path(TESTFILE).size
        http.upload(HTTP + '/action/uploadTest', { file: TESTFILE })
        await http.wait()
        ttrue(http.status == 200)
        http.close()

        const uploaded = new Path('../web/tmp').join(new Path(TESTFILE).basename)
        ttrue(uploaded.size == expectedSize)

        const uploadedBytes = await Bun.file(uploaded.toString()).arrayBuffer()
        const originalBytes = await Bun.file(TESTFILE).arrayBuffer()
        ttrue(Buffer.from(uploadedBytes).equals(Buffer.from(originalBytes)))
    } finally {
        await new Path(TESTFILE).remove()
        await new Path('../web/tmp').join(new Path(TESTFILE).basename).remove()
    }
} else {
    tskip('Upload not enabled')
}
