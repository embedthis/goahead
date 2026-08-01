/*
    cgi-content-length-honesty.tst - Regression test for CGI-declared Content-Length enforcement.

    The Content-Length a CGI program declared was copied into the response headers with atoi() and
    never compared against the bytes actually forwarded. A CGI that under-declared left the surplus
    in the stream, where a keep-alive client frames the response at the declared length and reads
    the surplus as the start of the next response -- a response-splitting and cache-poisoning
    primitive. A CGI that over-declared left the client waiting on bytes that never arrived, so the
    next response on the connection was read as their continuation. atoi() also silently accepted a
    negative or non-numeric value.

    The surplus must never reach the wire (closing afterwards cannot retract bytes the client has
    already buffered), a length that does not parse strictly must fall back to chunked framing, and
    any irreconcilable response must take the connection down rather than leave it desynchronised.

    The suite runs in two halves. The first drives responses small enough to arrive in one read, so
    it covers the parse and the reconciliation. The second drives responses larger than the 4097 byte
    read buffer, which is the only coverage of the forwarded count accumulating across reads and poll
    passes and of a declared length whose boundary falls mid-buffer; it also pins the overflow guard
    at its boundary and covers the redirect, non-parsed-header and duplicate-header shapes.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, teq } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Send an exact byte sequence and read until the server closes. Every case ends with a request
    carrying "Connection: close", so a server that correctly keeps the connection alive still
    reaches EOF without relying on timing.
 */
async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while (await s.read(response, -1) != null) {}
    } catch {
        /* Read timeout: connection left open with no further data. Treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

/*
    cgitest -r emits its argument as the complete response, verbatim -- no headers or body of
    cgitest's own, so the response can declare any Content-Length independent of what it writes.
    getArgv descapes the switch string once and splits it on whitespace, so the response text is
    double-encoded: %2520 survives the first descape as %20, and %250d%250a delivers a raw CRLF.
 */
function rawSwitch(encoded: string): string {
    return '-r%20' + encoded
}

/* Build a CGI response declaring "declared" as its Content-Length and writing "body" verbatim. */
function cgiResponse(declared: string, body: string): string {
    return rawSwitch('Content-Type:%2520text/plain%250d%250a' +
        'Content-Length:%2520' + declared + '%250d%250a%250d%250a' + body)
}

/*
    The -r form above is capped by cgitest's 1024 byte switch buffer, so it cannot reach the
    multi-read paths. cgitest -L declares a Content-Length verbatim and -B writes an exact number of
    body bytes independently of it, at any size. Byte N of that body is '0' + (N % 10), so a test can
    verify the offset at which a truncated body was cut, not merely how many bytes arrived.
 */
function cgiSized(declared: string, bodyBytes: number): string {
    return '-L%20' + declared + '%20-B%20' + bodyBytes
}

/* The body of the first response, up to the status line of any response that follows it. */
function firstBody(raw: string): string {
    let i = raw.indexOf('\r\n\r\n')
    if (i < 0) {
        return ''
    }
    let rest = raw.slice(i + 4)
    let j = rest.indexOf('HTTP/1.1 ')
    return j < 0 ? rest : rest.slice(0, j)
}

/* True if the body is the unbroken positional pattern cgitest -B emits. */
function patternOk(body: string): boolean {
    for (let i = 0; i < body.length; i++) {
        if (body.charCodeAt(i) != 48 + (i % 10)) {
            return false
        }
    }
    return true
}

/*
    Request the CGI over a keep-alive connection, then pipeline a second request for a static page.
    The second response only appears if the connection survived the first, so its presence or
    absence is the keep-alive assertion.
 */
function pipelined(switches: string): string {
    return 'GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nSWITCHES: ' + switches + '\r\n\r\n' +
        'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'
}

function countResponses(raw: string): number {
    return raw.split('HTTP/1.1 ').length - 1
}

if (thas('ME_GOAHEAD_CGI')) {

    /*
        Under-declared length. The CGI claims 5 bytes and writes 10. The five surplus bytes are the
        splitting primitive and must not reach the wire at all, and the connection must not be
        reused after a CGI has shown it cannot frame its own output.
     */
    let raw = await exchange(pipelined(cgiResponse('5', 'AAAAABBBBB')))
    ttrue(raw.indexOf('Content-Length: 5') >= 0, 'declared length must still frame the response')
    ttrue(raw.indexOf('AAAAA') >= 0, 'the declared body bytes must be delivered')
    ttrue(raw.indexOf('BBBBB') < 0, 'surplus bytes beyond the declared length must never be written')
    teq(countResponses(raw), 1, 'a desynchronised connection must not be kept alive')

    /*
        Over-declared length. The CGI claims 100 bytes and writes 10. The client is left waiting, so
        the connection must be closed rather than have the next response read as the continuation.
     */
    raw = await exchange(pipelined(cgiResponse('100', 'SHORTBODY!')))
    ttrue(raw.indexOf('SHORTBODY!') >= 0, 'the bytes the CGI did write must be delivered')
    teq(countResponses(raw), 1, 'a short-bodied response must not be kept alive')

    /*
        An honest CGI is unaffected: the declared length matches the bytes written, so the framing
        stands and the connection is reused for the pipelined request.
     */
    raw = await exchange(pipelined(cgiResponse('10', 'EXACTLY10!')))
    ttrue(raw.indexOf('EXACTLY10!') >= 0, 'an honest body must be delivered in full')
    ttrue(raw.indexOf('Hello /index.html') >= 0, 'an honest response must leave the connection reusable')
    teq(countResponses(raw), 2, 'an honest response must be kept alive')

    /*
        A Content-Length that does not parse strictly as a non-negative decimal is not a length.
        atoi() turned each of these into 0 or a truncated number and emitted it as fact; they must
        select chunked framing instead so the byte count actually written frames the response.
     */
    for (let bad of ['abc', '-5', '5x', '0x10', '+5', '99999999999999999999', '5%2520000']) {
        raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\n' +
            'SWITCHES: ' + cgiResponse(bad, 'BODYBYTES!') + '\r\nConnection: close\r\n\r\n')
        ttrue(raw.indexOf('Transfer-Encoding: chunked') >= 0,
            'unparsable Content-Length "' + bad + '" must fall back to chunked framing')
        ttrue(raw.indexOf('Content-Length:') < 0,
            'unparsable Content-Length "' + bad + '" must not be echoed to the client')
        ttrue(raw.indexOf('BODYBYTES!') >= 0,
            'the body must still be delivered for Content-Length "' + bad + '"')
    }

    /*
        A zero-length declaration is legitimate and must be honoured exactly: no body bytes, and the
        connection stays usable.
     */
    raw = await exchange(pipelined(cgiResponse('0', '')))
    ttrue(raw.indexOf('Content-Length: 0') >= 0, 'a zero Content-Length must be passed through')
    teq(countResponses(raw), 2, 'an empty but honest response must be kept alive')

    /*
        Everything above fits in a single 4097 byte read in a single poll pass, so it exercises the
        clamp only from a zero byte count. The cases below are larger than one read: they are the
        only coverage of txBytes accumulating across reads and across poll passes, and of a declared
        length whose boundary falls in the middle of a buffer rather than at its start.
     */

    /* An honest response larger than one read. The count must accumulate, not reset per read. */
    raw = await exchange(pipelined(cgiSized('20000', 20000)))
    ttrue(raw.indexOf('Content-Length: 20000') >= 0, 'a multi-read honest length must be passed through')
    teq(firstBody(raw).length, 20000, 'a multi-read honest body must be delivered in full')
    ttrue(patternOk(firstBody(raw)), 'a multi-read body must be delivered unmangled')
    teq(countResponses(raw), 2, 'a multi-read honest response must be kept alive')

    /*
        The cut lands mid-buffer: 10000 is inside the third read, so this is the only case where the
        clamp computes a partial write from an already-nonzero count. A body that is 10000 bytes long
        AND still matches the pattern proves the cut was taken at the right offset -- a clamp that
        reset or double-counted would deliver the right length from the wrong place in the stream.
     */
    raw = await exchange(pipelined(cgiSized('10000', 20000)))
    ttrue(raw.indexOf('Content-Length: 10000') >= 0, 'the declared length must frame the response')
    teq(firstBody(raw).length, 10000, 'a multi-read body must be truncated at exactly the declared length')
    ttrue(patternOk(firstBody(raw)), 'the truncated body must be cut at the declared offset, not resampled')
    teq(countResponses(raw), 1, 'a multi-read over-write must not be kept alive')

    /* Short body over several reads: the reconciliation runs on the accumulated total. */
    raw = await exchange(pipelined(cgiSized('20000', 5000)))
    teq(firstBody(raw).length, 5000, 'a multi-read short body must deliver what the CGI wrote')
    ttrue(patternOk(firstBody(raw)), 'a multi-read short body must be delivered unmangled')
    teq(countResponses(raw), 1, 'a multi-read short response must not be kept alive')

    /*
        The overflow guard, at its boundary. 2147483647 is exactly MAXSSIZE on a 32 bit target and
        well inside it on a 64 bit one, so it must be accepted either way -- it also proves the value
        is not being truncated through an int. 2^63 and 2^64 exceed MAXSSIZE on both and must be
        refused rather than wrapped.
     */
    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\n' +
        'SWITCHES: ' + cgiSized('2147483647', 10) + '\r\nConnection: close\r\n\r\n')
    ttrue(raw.indexOf('Content-Length: 2147483647') >= 0, 'a length at the ssize limit must be accepted intact')

    for (let huge of ['9223372036854775808', '18446744073709551616']) {
        raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\n' +
            'SWITCHES: ' + cgiSized(huge, 10) + '\r\nConnection: close\r\n\r\n')
        ttrue(raw.indexOf('Transfer-Encoding: chunked') >= 0,
            'an overflowing Content-Length "' + huge + '" must fall back to chunked framing')
        ttrue(raw.indexOf('Content-Length:') < 0,
            'an overflowing Content-Length "' + huge + '" must not be echoed to the client')
    }

    /* A Content-Length header with no value at all is not a length. */
    raw = await exchange('GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nSWITCHES: ' +
        rawSwitch('Content-Type:%2520text/plain%250d%250aContent-Length:%250d%250a%250d%250aBODYBYTES!') +
        '\r\nConnection: close\r\n\r\n')
    ttrue(raw.indexOf('Transfer-Encoding: chunked') >= 0, 'an empty Content-Length must fall back to chunked')
    ttrue(raw.indexOf('BODYBYTES!') >= 0, 'the body must still be delivered for an empty Content-Length')

    /*
        Two Content-Length headers is a malformed response rather than a length to reconcile, so it
        is covered with the other malformed shapes in cgi-response-framing.tst.
     */

    /*
        A redirect carries a body and a length like any other response, and the length is reconciled
        the same way. writeCgiHeaders takes a different path when a Location is present, so an honest
        redirect must still be framed and kept alive.
     */
    raw = await exchange(pipelined(cgiSized('10', 10) + '%20-l%20/index.html'))
    ttrue(raw.indexOf('Content-Length: 10') >= 0, 'a redirect must carry its declared length')
    ttrue(raw.indexOf('Location: /index.html') >= 0, 'the Location header must survive')
    teq(countResponses(raw), 2, 'an honest redirect must be kept alive')

    /*
        A non-parsed-header response prefixes the block with its own HTTP status line, which
        parseCgiHeaders strips before the control headers are collected. The declared length must
        still be found and enforced on that path.
     */
    raw = await exchange(pipelined('-n%20' + cgiSized('5', 20)))
    ttrue(raw.indexOf('Content-Length: 5') >= 0, 'an nph declared length must be found past the status line')
    teq(firstBody(raw).length, 5, 'an nph response must be truncated at its declared length')
    teq(countResponses(raw), 1, 'an nph over-write must not be kept alive')

    /* The server survives every case above */
    raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    ttrue(raw.indexOf('Hello /index.html') >= 0, 'server must still serve requests')

} else {
    tskip('CGI not enabled')
}
