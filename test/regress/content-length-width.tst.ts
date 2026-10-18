/*
    content-length-width.tst - The response Content-Length must be emitted at full width

    websWriteHeaders() takes an ssize length but emitted it as websWriteHeader(wp, "Content-Length",
    "%d", (int) length), while wp->txLen kept the full 64-bit value and governed the body. Header and
    body therefore disagreed by length - (length mod 2^32) for any response of 2 GiB or more. On a
    keep-alive connection the client or an intermediary cache reads Content-Length bytes and parses
    the remaining body as the start of the next response: response splitting and cache poisoning,
    with attacker-chosen bytes wherever any part of the file is attacker-influenced.

    The two boundaries asserted here:

      2^31     the (int) cast turns the value NEGATIVE   ("Content-Length: -2147483648")
      2^32 + 5 the (int) cast turns it into a tiny value ("Content-Length: 5")

    HEAD is used rather than GET so the assertion is on framing alone and the suite never moves
    gigabytes. Both branches of the emit are covered: the HEAD branch directly, and the GET branch
    via a Range request, which is served with a small 206 body but sizes its header from the same
    64-bit value.

    Fixtures are sparse: they cost inodes, not disk. Windows truncation is not sparse on every
    filesystem, so this axis is skipped there rather than writing 4 GiB to a CI runner.
 */

import { Uri, Config } from '@embedthis/ejscript'
import { teq, ttrue, tskip, tget } from '@embedthis/testme'
import { closeSync, openSync, ftruncateSync, rmSync } from 'node:fs'
import { connect } from 'node:net'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const httpUri = new Uri(HTTP)

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('sparse-file fixtures are not portable to the Windows CI filesystem')
} else {

const DOCS = join(import.meta.dir, '..', 'web')

/*
    Inspect raw header bytes. Stop after headers to avoid reading the large file body.
 */
function exchange(payload: string, headersOnly: boolean = false): Promise<string> {
    return new Promise((resolve) => {
        let response = ''
        let s = connect(Number(httpUri.port), httpUri.host, () => s.write(payload))
        s.setEncoding('latin1')
        s.setTimeout(10000, () => s.destroy())
        s.on('data', (data: string) => {
            response += data
            if (headersOnly && response.includes('\r\n\r\n')) {
                s.destroy()
            }
        })
        s.on('error', () => s.destroy())
        s.on('close', () => resolve(response))
    })
}

function contentLength(raw: string): string {
    let m = raw.match(/\r\nContent-Length: *([^\r\n]*)\r\n/i)
    return m ? m[1] : ''
}

function makeSparse(name: string, size: number): string {
    let path = join(DOCS, name)
    let fd = openSync(path, 'w')
    ftruncateSync(fd, size)
    closeSync(fd)
    return path
}

const CASES = [
    /* (int) 2147483648 is -2147483648: a negative length, which is not even syntactically valid */
    { name: 'cl-2g.dat', size: 2147483648 },
    /* (int) 4294967301 is 5: the reproduction from the audit report */
    { name: 'cl-4g.dat', size: 4294967301 },
]

let made: string[] = []
try {
    for (let c of CASES) {
        made.push(makeSparse(c.name, c.size))

        let raw = await exchange(`HEAD /${c.name} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`)
        teq(contentLength(raw), String(c.size), `HEAD must declare the full length for a ${c.size}-byte file`)

        /*
            A Range request exercises the non-HEAD branch of the same emit. The 206 body is small, but
            Content-Range carries the full instance length from the same 64-bit value, so a truncating
            conversion anywhere on that path is still visible.
         */
        raw = await exchange(`GET /${c.name} HTTP/1.1\r\nHost: localhost\r\n` +
            `Range: bytes=0-9\r\nConnection: close\r\n\r\n`, true)
        ttrue(!contentLength(raw).startsWith('-'),
            `a ranged GET of a ${c.size}-byte file must not declare a negative length`)
        ttrue(!raw.includes(`/${c.size % 4294967296}\r\n`) || raw.includes(`/${c.size}`),
            `a ranged GET of a ${c.size}-byte file must not declare a truncated instance length`)
    }

    /*
        The framing must also survive keep-alive, which is where the split is exploitable. Two
        pipelined HEADs on one connection must yield two responses, each declaring the full length -
        the pre-fix header let the peer resynchronise in the wrong place.
     */
    let raw = await exchange('HEAD /cl-4g.dat HTTP/1.1\r\nHost: localhost\r\n\r\n' +
        'HEAD /cl-4g.dat HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
    let lengths = raw.match(/Content-Length: *(\d+)/gi) || []
    teq(lengths.length, 2, 'two pipelined HEADs must yield two Content-Length headers')
    for (let l of lengths) {
        ttrue(l.includes('4294967301'), 'each keep-alive response must declare the full length')
    }
} finally {
    for (let path of made) {
        rmSync(path, { force: true })
    }
}

}
