/*
    cgi-connection-promise.tst.ts - CGI Connection headers agree with socket lifetime
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

/*
    Double-encode raw CGI output because getArgv and cgitest each decode once.
 */
function cgiResponse(declared: string, body: string): string {
    return '-r%20Content-Type:%2520text/plain%250d%250a' +
        'Content-Length:%2520' + declared + '%250d%250a%250d%250a' + body
}

/*
    Pipeline a second request to verify the advertised connection persistence.
 */
function pipelined(switches: string): string {
    return 'GET /cgi-bin/cgitest HTTP/1.1\r\nHost: x\r\nSWITCHES: ' + switches + '\r\n\r\n' +
        'GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n'
}

function firstHeaders(raw: string): string {
    let end = raw.indexOf('\r\n\r\n')
    return (end < 0 ? raw : raw.slice(0, end)).toLowerCase()
}

function responseCount(raw: string): number {
    return raw.split('HTTP/1.1 ').length - 1
}

if (thas('ME_GOAHEAD_CGI')) {

    let raw = await exchange(pipelined(cgiResponse('5', 'AAAAABBBBB')))
    ttrue(firstHeaders(raw).contains('connection: close'),
          `an over-writing CGI must be framed as closing, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 1, 'the connection must actually close, matching the header')

    /*
        Length shortfall is known only after child exit.
     */
    raw = await exchange(pipelined(cgiResponse('100', 'SHORTBODY!')))
    ttrue(firstHeaders(raw).contains('connection: close'),
          `an under-writing CGI must be framed as closing, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 1, 'the connection must actually close, matching the header')

    raw = await exchange(pipelined(cgiResponse('10', 'EXACTLY10!')))
    ttrue(firstHeaders(raw).contains('connection: keep-alive'),
          `an honest CGI must be promised persistence, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 2, 'the connection must actually be reused, matching the header')

    /*
        Unknown-length CGI responses use server-controlled framing and can remain persistent.
     */
    raw = await exchange(pipelined('-r%20Content-Type:%2520text/plain%250d%250a%250d%250aNOLENGTH!!'))
    ttrue(firstHeaders(raw).contains('connection: keep-alive'),
          `a chunked CGI response must keep persistence, got [${firstHeaders(raw)}]`)
    ttrue(responseCount(raw) == 2, 'a chunked CGI response must not close the connection')

    raw = await exchange('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
    ttrue(raw.contains('200 OK'), 'the server must still serve afterwards')
}
