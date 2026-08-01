/*
    cgi-response-framing.tst - Regression test for CGI response framing driven by status and method.

    Sibling defects to the Content-Length enforcement covered by cgi-content-length-honesty.tst.
    There the framing was broken by the length the CGI declared; here it is broken by the status it
    declared or by the method of the request, so none of it is caught by reconciling a length.

    - A 204 or 304 carries no body, and websWriteHeaders emits no Content-Length for those statuses.
      Chunking is only enabled when the length is unknown, so the body a CGI wrote after such a
      status went onto the wire with no framing header of any kind, on a connection left alive. The
      client frames the response as empty and reads the body as the start of the next response: a
      complete response-splitting and cache-poisoning primitive needing only a Status line.
    - The status itself was atoi()'d, so "abc" became 0 and was emitted as "HTTP/1.1 0 ...".
    - A HEAD request ran the CGI and forwarded its output as a response body.
    - Two Content-Length headers with different values were resolved by taking whichever the CGI
      wrote last, making the framing depend on write order.

    A response the gateway cannot interpret must be answered on the gateway's own behalf, carrying
    none of the CGI's body and none of its remaining headers. A response that legitimately carries no
    body must stay honestly framed and therefore reusable -- suppressing a body is the framing
    working, not a fault, and must not close the connection.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, teq } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

/*
    Send an exact byte sequence and read until the server closes. Every case ends with a request
    carrying "Connection: close", so a server that correctly keeps the connection alive still reaches
    EOF without relying on timing.
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
    cgitest -r emits its argument as the complete response, verbatim. getArgv descapes the switch
    string once and splits it on whitespace, so the response is double-encoded: %2520 survives the
    first descape as %20, and %250d%250a delivers a raw CRLF.
 */
function rawSwitch(encoded: string): string {
    return '-r%20' + encoded
}

/* Request the CGI, then pipeline a static request. The second response only arrives if the
   connection survived the first, so its presence is the keep-alive assertion. */
function pipelined(switches: string, method: string = 'GET'): string {
    return method + ' /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nSWITCHES: ' + switches + '\r\n\r\n' +
        'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'
}

function countResponses(raw: string): number {
    return raw.split('HTTP/1.1 ').length - 1
}

/* Headers of the first response. */
function firstHeaders(raw: string): string {
    let i = raw.indexOf('\r\n\r\n')
    return i < 0 ? raw : raw.slice(0, i)
}

/* Body of the first response, up to the status line of any response that follows it. */
function firstBody(raw: string): string {
    let i = raw.indexOf('\r\n\r\n')
    if (i < 0) {
        return ''
    }
    let rest = raw.slice(i + 4)
    let j = rest.indexOf('HTTP/1.1 ')
    return j < 0 ? rest : rest.slice(0, j)
}

if (thas('ME_GOAHEAD_CGI')) {

    /*
        A status that carries no body. The CGI writes one anyway, and in the 204 case also declares a
        length for it. Nothing of that body may reach the wire: with no Content-Length emitted for
        these statuses and no chunked framing, a single byte of it is the splitting primitive. The
        framing that results is honest, so the connection must remain reusable.
     */
    for (let c of [{ status: '204', declared: 'Content-Length:%252010%250d%250a' },
                   { status: '204', declared: '' },
                   { status: '304', declared: 'Content-Length:%25207%250d%250a' },
                   { status: '304', declared: '' }]) {
        let raw = await exchange(pipelined(rawSwitch('Content-Type:%2520text/plain%250d%250a' +
            'Status:%2520' + c.status + '%250d%250a' + c.declared + '%250d%250aBODYBYTES!')))
        let label = c.status + (c.declared ? ' with a declared length' : ' with no declared length')
        ttrue(firstHeaders(raw).indexOf('HTTP/1.1 ' + c.status) == 0, label + ' must keep its status')
        teq(firstBody(raw), '', label + ' must carry no body bytes at all')
        ttrue(firstHeaders(raw).indexOf('Content-Length:') < 0, label + ' must emit no Content-Length')
        ttrue(firstHeaders(raw).indexOf('Transfer-Encoding') < 0,
            label + ' must emit no chunked framing, whose trailer would itself be body bytes')
        teq(countResponses(raw), 2, label + ' is honestly framed and must stay reusable')
    }

    /*
        A Status the gateway cannot interpret. atoi() turned each of these into a number and emitted
        it as the response code -- "abc" produced the literal status line "HTTP/1.1 0". The gateway
        must answer on its own behalf instead. 1XX is refused with them: an informational status is
        interim, so a client given one as the final response waits for a second response that will
        never come, and a CGI cannot emit an interim response through the gateway anyway.
     */
    for (let bad of ['abc', '99', '1000', '200x', '0', '-200', '2', '20', '100', '199']) {
        let raw = await exchange(pipelined(rawSwitch('Content-Type:%2520text/plain%250d%250a' +
            'Status:%2520' + bad + '%250d%250a%250d%250aBODYBYTES!')))
        ttrue(firstHeaders(raw).indexOf('HTTP/1.1 500') == 0,
            'Status "' + bad + '" cannot be a final response status and must yield 500')
        teq(firstBody(raw), '', 'a rejected response must not forward the CGI body')
        teq(countResponses(raw), 2, 'a rejected response is honestly framed and must stay reusable')
    }

    /*
        A well-formed status is passed through untouched, with or without a reason phrase. 711 is
        outside the classes RFC 9110 defines, but a CGI choosing its own code is long-standing
        documented behaviour here (basic/cgi.tst asserts it) and an unclassified code misframes
        nothing, so three digits is the only bound applied above 1XX.
     */
    for (let good of ['200', '404', '404%2520Gone', '503', '599', '711']) {
        let raw = await exchange(pipelined(rawSwitch('Content-Type:%2520text/plain%250d%250a' +
            'Status:%2520' + good + '%250d%250a%250d%250abody')))
        let code = good.slice(0, 3)
        ttrue(firstHeaders(raw).indexOf('HTTP/1.1 ' + code) == 0, 'Status "' + good + '" must be passed through')
        ttrue(firstBody(raw).indexOf('body') >= 0, 'a valid status must still deliver the body')
    }

    /*
        A response the gateway rejected must carry none of the CGI's other headers either. They were
        written to accompany a response the client is not going to receive, and a Set-Cookie or a
        cache directive surviving into the gateway's own error response is the CGI still steering a
        client it no longer has a valid response for.
     */
    let raw = await exchange(pipelined(rawSwitch('Content-Type:%2520text/plain%250d%250a' +
        'Status:%2520abc%250d%250aSet-Cookie:%2520session=attacker%250d%250a%250d%250abody')))
    ttrue(firstHeaders(raw).indexOf('HTTP/1.1 500') == 0, 'a bad Status must yield 500')
    ttrue(raw.indexOf('session=attacker') < 0, 'a rejected response must not forward the CGI headers')

    /*
        HEAD. The CGI is run and writes a body; none of it may be forwarded. Where the CGI declared a
        length the header keeps it, because reporting what the equivalent GET would return is the
        point of a HEAD; where it did not, the length is declared zero rather than left unknown,
        since an unknown length selects chunked framing and the chunk trailer is itself body bytes.
     */
    raw = await exchange(pipelined('-L%204%20-B%204', 'HEAD'))
    ttrue(raw.indexOf('Content-Length: 4') >= 0, 'HEAD must report the length the CGI declared')
    teq(firstBody(raw), '', 'HEAD must carry no body')
    teq(countResponses(raw), 2, 'HEAD must leave the connection reusable')

    raw = await exchange(pipelined('-B%2020', 'HEAD'))
    ttrue(firstHeaders(raw).indexOf('Transfer-Encoding') < 0, 'HEAD must never select chunked framing')
    teq(firstBody(raw), '', 'HEAD with no declared length must still carry no body')
    teq(countResponses(raw), 2, 'HEAD with no declared length must leave the connection reusable')

    /*
        Two Content-Length headers. RFC 9110 makes differing values an unrecoverably invalid message;
        taking the last one made the framing depend on the order the CGI wrote them. Repeating the
        same value is permitted and must still be honoured.
     */
    raw = await exchange(pipelined(rawSwitch('Content-Type:%2520text/plain%250d%250a' +
        'Content-Length:%25205%250d%250aContent-Length:%2520100%250d%250a%250d%250aABCDEFGHIJ')))
    ttrue(firstHeaders(raw).indexOf('HTTP/1.1 500') == 0, 'conflicting Content-Length must yield 500')
    teq(firstBody(raw), '', 'conflicting Content-Length must not forward a body framed by either value')
    teq(countResponses(raw), 2, 'a rejected response is honestly framed and must stay reusable')

    raw = await exchange(pipelined(rawSwitch('Content-Type:%2520text/plain%250d%250a' +
        'Content-Length:%252010%250d%250aContent-Length:%252010%250d%250a%250d%250aABCDEFGHIJ')))
    ttrue(raw.indexOf('Content-Length: 10') >= 0, 'a repeated identical Content-Length must be collapsed')
    teq(firstBody(raw), 'ABCDEFGHIJ', 'a repeated identical Content-Length must still frame the body')
    teq(countResponses(raw), 2, 'a repeated identical Content-Length must stay reusable')

    /* The server survives every case above */
    raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    ttrue(raw.indexOf('Hello /index.html') >= 0, 'server must still serve requests')

} else {
    tskip('CGI not enabled')
}
