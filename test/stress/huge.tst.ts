/*
    huge.tst - Transfer a file larger than 4 GB and verify the byte count and the advertised length.

    This is the suite's only exercise of a response beyond the 32-bit range, which is where a
    truncated length or offset shows up. The body is streamed and counted rather than buffered: the
    ejscript Http class reads a whole response into a string, so it cannot carry 5 GB. fetch() hands
    back a stream that can be drained a chunk at a time.

    The file is generated on first use and left in place afterwards - generating it costs more than
    the transfer it feeds. test/web/huge.txt is git-ignored.
 */

import { Path } from '@embedthis/ejscript'
import { tdepth, tget, tinfo, tskip, ttrue } from '@embedthis/testme'

const DEPTH = 5
const HUGE = '../web/huge.txt'
const BLOCK = 1024 * 1024
const SIZE = 5 * 1024 * 1024 * 1024

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const URL = (HTTP.includes('://') ? HTTP : 'http://' + HTTP) + '/huge.txt'

if (tdepth() >= DEPTH) {
    /*
        Create the test file. A run that finds a file of the right size reuses it.
     */
    if (!new Path(HUGE).exists || new Path(HUGE).size < SIZE) {
        tinfo('  [Generate] ' + SIZE / (1024 * 1024 * 1024) + 'GB test file "' + HUGE + '" ...')
        /*
            A repeating 64-byte line replicated into a 1 MB block. Real bytes rather than a sparse
            hole, so the server reads as many as it writes. Both sizes divide SIZE exactly.
         */
        const line = '0123456789abcdef'.repeat(3) + '0123456789abcde\n'
        const block = new TextEncoder().encode(line.repeat(BLOCK / line.length))
        const writer = Bun.file(HUGE).writer()
        for (let written = 0; written < SIZE; written += block.length) {
            writer.write(block)
            await writer.flush()
        }
        await writer.end()
    }
    ttrue(new Path(HUGE).size == SIZE)

    /*
        Test Http get for a huge file
     */
    const started = Date.now()
    const response = await fetch(URL)
    ttrue(response.status == 200)

    /*
        The advertised length must survive the 32-bit boundary intact. A truncated Content-Length
        desynchronises the connection: the peer stops reading at the advertised count and takes the
        rest of the body for the next response.
     */
    ttrue(Number(response.headers.get('content-length')) == SIZE)

    let total = 0
    const reader = response.body.getReader()
    for (;;) {
        const {done, value} = await reader.read()
        if (done) break
        total += value.length
    }
    const elapsed = (Date.now() - started) / 1000

    ttrue(total == SIZE)
    tinfo('  [Transfer] ' + total + ' bytes in ' + elapsed.toFixed(1) + 's (' +
        (total / elapsed / (1024 * 1024)).toFixed(1) + ' MB/s)')

} else {
    tskip('Runs at depth ' + DEPTH)
}
