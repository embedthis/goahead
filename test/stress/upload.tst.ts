/*
    upload.tst - Stress test uploads
 */

import { Http, File, Path, ByteArray, Cmd } from 'ejscript'
import { tdepth, tget, thas, tskip, ttrue } from 'testme'

// Simple hash function for generating unique filenames
function hashcode(value: any): number {
    const str = String(value)
    let hash = 0
    for (let i = 0; i < str.length; i++) {
        hash = ((hash << 5) - hash) + str.charCodeAt(i)
        hash = hash & hash // Convert to 32-bit integer
    }
    return Math.abs(hash)
}

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const TESTFILE = 'upload-' + hashcode(process.pid) + '.tdat'

/* This test requires chunking support */
if (thas('ME_GOAHEAD_UPLOAD')) {

    let http: Http = new Http

    /* Depths:    0  1  2  3   4   5   6    7    8    9    */
    var sizes = [ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 ]

    //  Create test data
    let buf = new ByteArray
    for (let i = 0; i < 64; i++) {
        for (let j = 0; j < 15; j++) {
            buf.writeByte('A'.charCodeAt(0) + (j % 26))
        }
        buf.writeByte('\n'.charCodeAt(0))
    }

    //  Create test data file
    let f = File(TESTFILE).open({mode: 'w'})
    for (let i = 0; i < (sizes[tdepth()] * 1024); i++) {
        f.write(buf)
    }
    f.close()

    try {
        let size = await Path(TESTFILE).size
        http.upload(HTTP + '/action/uploadTest', { file: TESTFILE })
        await http.wait()
        ttrue(http.status == 200)
        http.close()
        let uploaded = Path('../web/tmp').join(Path(TESTFILE).basename)
        ttrue(await uploaded.size == size)
        Cmd.sh('diff ' + uploaded + ' ' + TESTFILE)
    }
    finally {
        Path(TESTFILE).remove()
    }

} else {
    tskip('Upload not enabled')
}
