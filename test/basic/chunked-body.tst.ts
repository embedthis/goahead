/*
    chunked-body.tst - Chunked request body reassembly

    Replaces basic/callback.tst.ts. That test drove Http.Read / Http.Write callbacks and a chunked
    upload against /big.asp and /f.asp. The callback API does not exist in the published
    @embedthis/ejscript client and neither fixture exists any more, so the whole body had been
    wrapped in "if (false)" -- it reported PASS for years while executing nothing.

    What it was reaching for that nothing else covers is the chunked request body happy path.
    basic/framing.tst.ts and basic/smuggle.tst.ts cover malformed chunk sizes and TE/CL desync, and
    the regress/cgi-* tests cover chunked CGI responses, but no test asserted that a well-formed
    chunked request body is reassembled correctly and handed to a handler.

    The client normalises framing and cannot be made to split a body at chosen offsets, so this
    speaks raw TCP. /action/showTest echoes every parsed form variable as "name=value", so a
    reassembly error shows up as a missing or corrupted variable rather than a status code.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, ttrue, tcontains } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const uri = new Uri(HTTP)
const ADDR = uri.host + ":" + uri.port

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

function chunkedPost(chunks: string[]): string {
    let body = ''
    for (let c of chunks) {
        body += c.length.toString(16) + '\r\n' + c + '\r\n'
    }
    body += '0\r\n\r\n'
    return 'POST /action/showTest HTTP/1.1\r\n' +
           'Host: ' + ADDR + '\r\n' +
           'Connection: close\r\n' +
           'Content-Type: application/x-www-form-urlencoded\r\n' +
           'Transfer-Encoding: chunked\r\n\r\n' + body
}

/*
    Split mid-name and mid-value so a reassembly that loses or reorders a chunk cannot still
    produce the right variables.
 */
let r = await exchange(chunkedPost(['alpha', '=one', '&bet', 'a=t', 'wo']))
ttrue(r.contains('200 OK'))
tcontains(r, 'alpha=one')
tcontains(r, 'beta=two')

/*  Single chunk carrying the whole body. */
r = await exchange(chunkedPost(['alpha=one&beta=two']))
ttrue(r.contains('200 OK'))
tcontains(r, 'alpha=one')
tcontains(r, 'beta=two')

/*  Many small chunks - one byte each. */
let value = 'abcdefghij'
r = await exchange(chunkedPost(('alpha=' + value).split('')))
ttrue(r.contains('200 OK'))
tcontains(r, 'alpha=' + value)

/*  A zero-length chunk before the terminator must not truncate the body. */
r = await exchange('POST /action/showTest HTTP/1.1\r\n' +
                   'Host: ' + ADDR + '\r\n' +
                   'Connection: close\r\n' +
                   'Content-Type: application/x-www-form-urlencoded\r\n' +
                   'Transfer-Encoding: chunked\r\n\r\n' +
                   'a\r\nalpha=one&\r\n8\r\nbeta=two\r\n0\r\n\r\n')
ttrue(r.contains('200 OK'))
tcontains(r, 'alpha=one')
tcontains(r, 'beta=two')
