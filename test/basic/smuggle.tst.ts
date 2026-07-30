/*
    smuggle.tst - HTTP request smuggling (CWE-444) framing tests

    Exercises RFC 9112 6.1 message-framing enforcement in parseHeaders(). The Ejscript Http client normalises
    framing headers and cannot emit the conflicting/obfuscated combinations under test, so these cases speak raw
    TCP via Socket and count "HTTP/1." status lines in the response stream. A framing desync would surface as two
    responses on one connection (the smuggled request serviced); a correct server returns a single rejection.

    Also re-runs the reporter's CL.TE proof-of-concept and its TE.CL mirror as permanent security assertions.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, ttrue, teq } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const uri = new Uri(HTTP)
const ADDR = uri.host + ":" + uri.port

/*
    Send an exact byte sequence, read until the server closes the connection, and return the number of HTTP
    responses seen plus their status codes. Every case below closes the connection (rejections via WEBS_CLOSE,
    regressions via "Connection: close"), so the read loop terminates on EOF without relying on timing.
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
    CL.TE (Content-Length first): both framings present. Must be rejected 400 and the connection closed, so the
    bytes after the chunked terminator are never serviced as a second request.
 */
let r = await exchange(
    "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n" +
    "Connection: keep-alive\r\n\r\n0\r\n\r\nGET /SMUGGLED HTTP/1.1\r\nHost: x\r\n\r\n")
teq(r.count, 1, "CL.TE must yield a single response, not a smuggled second request")
teq(r.statuses[0], 400, "CL.TE must be rejected 400")

/*
    TE.CL (Transfer-Encoding first): same conflict, opposite header order. Rejection must be order-independent.
 */
r = await exchange(
    "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n0\r\n\r\n")
teq(r.count, 1, "TE.CL must yield a single response")
teq(r.statuses[0], 400, "TE.CL must be rejected 400 regardless of header order")

/*
    Obfuscated/list transfer coding with no Content-Length. GoAhead decodes only "chunked"; a coding list is
    unsupported -> 501. It must never silently fall back to Content-Length framing (the TE.CL smuggling vector).
 */
r = await exchange(
    "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip, chunked\r\n\r\n" +
    "0\r\n\r\nGET /SMUGGLED HTTP/1.1\r\nHost: x\r\n\r\n")
teq(r.count, 1, "list transfer coding must not smuggle a second request")
teq(r.statuses[0], 501, "unsupported transfer coding must be rejected 501")

/*
    Transfer-Encoding is an HTTP/1.1 mechanism; it must be rejected on an HTTP/1.0 message.
 */
r = await exchange(
    "POST / HTTP/1.0\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n")
teq(r.statuses[0], 400, "Transfer-Encoding on HTTP/1.0 must be rejected 400")

/*
    A duplicate Content-Length header is ambiguous framing and must be rejected.
 */
r = await exchange(
    "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello")
teq(r.statuses[0], 400, "duplicate Content-Length must be rejected 400")

/*
    Content-Length must be a single run of digits; a non-digit tail ("5x") must be rejected, not parsed as 5.
 */
r = await exchange(
    "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 5x\r\n\r\nhello")
teq(r.statuses[0], 400, "non-digit Content-Length must be rejected 400")

/*
    Regression: a well-formed chunked POST (single "chunked" coding, no Content-Length) still succeeds.
 */
r = await exchange(
    "POST /action/test HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n" +
    "Connection: close\r\n\r\n9\r\nSome data\r\n0\r\n\r\n")
teq(r.statuses[0], 200, "well-formed chunked POST must still succeed")

/*
    Regression: a well-formed Content-Length POST still succeeds (strict digit parse accepts normal values).
 */
r = await exchange(
    "POST /action/test HTTP/1.1\r\nHost: x\r\nContent-Length: 9\r\n" +
    "Connection: close\r\n\r\nSome data")
teq(r.statuses[0], 200, "well-formed Content-Length POST must still succeed")

/*
    Regression: legitimate keep-alive pipelining of independent requests still works. The fix rejects ambiguous
    framing, not connection reuse.
 */
r = await exchange(
    "GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n" +
    "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
teq(r.count, 2, "legitimate pipelining must service both requests")
ttrue(r.statuses[0] == 200 && r.statuses[1] == 200, "both pipelined GETs must return 200")

/*
    Security assertion: TE.CL mirror of the reporter PoC (obfuscated Transfer-Encoding + Content-Length: 0 +
    trailing request). The trailing bytes must never be serviced as a smuggled request.
 */
r = await exchange(
    "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip, chunked\r\nContent-Length: 0\r\n\r\n" +
    "GET /SMUGGLED HTTP/1.1\r\nHost: x\r\n\r\n")
teq(r.count, 1, "TE.CL mirror PoC must yield a single response, no smuggled request")
