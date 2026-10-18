/*
    bad-path.tst - A request line whose target is neither an origin-form path nor an absolute URI

    "OPTIONS sip:nm SIP/2.0" is the SIP OPTIONS probe that scanners spray at anything listening. It
    is a valid request line shape for SIP and an invalid one for HTTP: the target is a bare URI with
    no authority and no leading '/', and the version token is not HTTP/x.y.

    The test previously sent this and read to end of stream without asserting anything at all, so it
    could only fail if the read threw. A server that answered 200, that served a file, or that
    desynchronised the connection would all have passed it. Assert the actual contract: the request
    is rejected, it is rejected as a client error, and the server is still healthy afterwards.
 */

import { Http, Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, ttrue, tcontains } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ":" + uri.port

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(8192, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange. */
    }
    s.close()
    return response.toString()
}

/*
    The original probe. Rejected, and rejected as a client error rather than a server error: a 5xx
    here would mean the malformed target reached code that was not expecting it.
 */
let r = await exchange('OPTIONS sip:nm SIP/2.0\r\nContent-Length: 0\r\nContact: \r\nAccept: application/*\r\n\r\n')
ttrue(r.length > 0, 'server closed without responding to the SIP probe')
tcontains(r, '400 Bad Request')
ttrue(!r.contains('500 '), 'malformed request target produced a server error')

/*  A bare non-origin-form target carrying a valid HTTP version is equally invalid. */
r = await exchange('GET sip:nm HTTP/1.0\r\n\r\n')
tcontains(r, '400 Bad Request')

/*
    Liveness: the probes above must not have wedged the event loop or left the listener in a bad
    state. GoAhead is single threaded, so a hang here is a denial of service for every client.
 */
let http: Http = new Http
http.get(HTTP + '/index.html')
await http.wait()
ttrue(http.status == 200, 'server unhealthy after malformed request targets')
http.close()

/* Absolute HTTP targets without a path resolve to the root, while malformed targets above stay rejected. */
r = await exchange('GET http://localhost HTTP/1.0\r\n\r\n')
tcontains(r, '302 Redirect')
tcontains(r, '/index.html')
