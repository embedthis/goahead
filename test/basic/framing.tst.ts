/*
    framing.tst - HTTP request smuggling (CWE-444) via HEAD body retention and chunk-size integer wrap

    Covers the two novel root causes tracked by issue #0025 / GOAHEAD-SA-2026-0010:

      1. HEAD body retention - a HEAD request that declares a Content-Length must have its body drained before the
         keep-alive connection is reused. Previously the declared body was left in the receive buffer and re-parsed
         as the next request.
      2. Chunk-size integer wrap - a chunk-size line that overflows 32 bits (e.g. 100000000) must be rejected, not
         wrapped to zero and mistaken for the terminating chunk. hextoi() now saturates and filterChunkData()
         upper-bounds the size against the request-body limit.

    The Ejscript Http client normalises framing and cannot emit a HEAD body or a bogus chunk-size line, so these
    cases speak raw TCP via Socket and count "HTTP/1." status lines. A framing desync surfaces as two responses on
    one connection (the smuggled request serviced); a correct server returns a single response or a single 400.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, ttrue, teq } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const uri = new Uri(HTTP)
const ADDR = uri.host + ":" + uri.port

/*
    Send an exact byte sequence, read until the server closes the connection (or a read timeout on a connection
    left open), and return the number of HTTP responses seen plus their status codes. Counting status lines is how
    the reporter's PoC distinguishes one response from two.
 */
async function exchange(payload: string): Promise<{count: number, statuses: number[], raw: string}> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    let n
    try {
        while ((n = await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: connection left open with no further data. Treat as end of exchange. */
    }
    s.close()
    let raw = response.toString()
    let statuses: number[] = []
    let re = /HTTP\/1\.[01] (\d{3})/g
    let m
    while ((m = re.exec(raw)) != null) {
        statuses.push(parseInt(m[1]))
    }
    return {count: statuses.length, statuses, raw}
}

/*
    HEAD body retention (reporter PoC --mode head-cl). A HEAD that declares a Content-Length whose body is itself a
    request. The declared body must be drained as the HEAD body, never serviced as a smuggled request. A real
    trailing request (with Connection: close) follows the declared body to close the connection deterministically:

      - Fixed:   the declared body is drained, then the trailing GET is serviced -> two responses (HEAD + trailing).
      - Unpatched: the declared body is serviced as a smuggled request too -> three responses.
 */
let body = "GET /alive.html HTTP/1.1\r\nHost: x\r\nConnection: keep-alive\r\n\r\n"
let trailing = "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"
let r = await exchange(
    "HEAD /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: " + body.length + "\r\n" +
    "Connection: keep-alive\r\n\r\n" + body + trailing)
teq(r.count, 2, "HEAD declared body must be drained, not serviced as a smuggled request")
ttrue(r.statuses[0] == 200 && r.statuses[1] == 200, "HEAD and the trailing GET must each return 200")

/*
    Chunk-size 32-bit wrap (reporter PoC --mode chunk-wrap). 100000000 == 2^32 wraps to 0 in an unguarded 32-bit
    accumulator and would be mistaken for the terminating chunk. It must be rejected 400 and the connection closed
    so the trailing GET is never serviced.
 */
r = await exchange(
    "POST /action/test HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n" +
    "100000000\r\nGET /alive.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
teq(r.count, 1, "chunk-size 100000000 must yield a single response, no smuggled request")
teq(r.statuses[0], 400, "chunk-size 100000000 (2^32 wrap) must be rejected 400")

/*
    Chunk-size 100000001 - overflows 32 bits to a small non-zero value in an unguarded accumulator; must saturate
    and be rejected.
 */
r = await exchange(
    "POST /action/test HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n" +
    "100000001\r\nGET /alive.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
teq(r.count, 1, "chunk-size 100000001 must yield a single response")
teq(r.statuses[0], 400, "chunk-size 100000001 (32-bit overflow) must be rejected 400")

/*
    Chunk-size FFFFFFFF - representable in 32 bits (no wrap) but far above the request-body limit; must be rejected
    by the upper-bound check.
 */
r = await exchange(
    "POST /action/test HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n" +
    "FFFFFFFF\r\nGET /alive.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
teq(r.count, 1, "oversized chunk-size FFFFFFFF must yield a single response")
teq(r.statuses[0], 400, "oversized chunk-size FFFFFFFF must be rejected 400")

/*
    Cumulative chunked body cap. A chunked body assembled from several in-range chunks must not exceed the
    request-body limit (the test server raises postLimit to 16 MB via websSetPostLimit). A first tiny chunk is
    accepted, then a second chunk *declares* a size that carries the running total past the limit: it must be
    rejected 413 + close at the size line, before that chunk's data is read. Declaring (not sending) the oversize
    chunk is exactly the "reject before draining" property, and it keeps the test payload tiny. Built as a helper
    so the buffered (/action/test) and streaming CGI targets share the shape.
 */
const LIMIT = 16 * 1024 * 1024              /* matches websSetPostLimit() in src/test.c */
function chunkedOverLimit(path: string): string {
    return "POST " + path + " HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n" +
        "1\r\nA\r\n" +                          /* chunk 1: running total = 1 (accepted) */
        LIMIT.toString(16) + "\r\n"             /* chunk 2 declares 16 MB -> total > limit -> 413 at size parse */
}

r = await exchange(chunkedOverLimit("/action/test"))
teq(r.count, 1, "cumulative chunked body over limit must yield a single response")
teq(r.statuses[0], 413, "cumulative chunked body over the request limit must be rejected 413")

/*
    Same cumulative cap for a chunked POST routed to CGI. websProcessCgiData continuously drains wp->input to the
    CGI process, so the per-buffer guard never trips; the cumulative counter in filterChunkData is what prevents an
    unbounded body being streamed to the CGI target.
 */
r = await exchange(chunkedOverLimit("/cgi-bin/cgitest"))
teq(r.count, 1, "cumulative chunked body to CGI over limit must yield a single response")
teq(r.statuses[0], 413, "chunked CGI body over the request limit must be rejected 413, not streamed unbounded")

/*
    Regression: a normal bodyless HEAD of a static file still returns 200 headers-only (draining logic must not
    alter the no-body case).
 */
r = await exchange("HEAD /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
teq(r.count, 1, "normal bodyless HEAD must return one response")
teq(r.statuses[0], 200, "normal bodyless HEAD must still succeed 200")

/*
    Regression: a well-formed chunked POST (including the literal-0 terminator) still succeeds and decodes.
 */
r = await exchange(
    "POST /action/test HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n" +
    "9\r\nSome data\r\n0\r\n\r\n")
teq(r.statuses[0], 200, "well-formed chunked POST must still succeed 200")

/*
    Regression: a well-formed Content-Length POST still succeeds.
 */
r = await exchange(
    "POST /action/test HTTP/1.1\r\nHost: x\r\nContent-Length: 9\r\nConnection: close\r\n\r\nSome data")
teq(r.statuses[0], 200, "well-formed Content-Length POST must still succeed 200")

/*
    Regression: legitimate keep-alive pipelining of independent GETs still works. The fix tightens body accounting,
    not connection reuse.
 */
r = await exchange(
    "GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n" +
    "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
teq(r.count, 2, "legitimate pipelining must service both requests")
ttrue(r.statuses[0] == 200 && r.statuses[1] == 200, "both pipelined GETs must return 200")
