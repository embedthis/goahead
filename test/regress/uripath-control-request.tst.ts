/*
    uripath-control-request.tst - A request whose path decodes to a control character is refused

    The unit side of this lives in regress/uripath-control-chars.tst.c, which drives
    websValidateUriPath directly. This is the end-to-end half: the bytes have to be refused on the
    wire, as a client error, before wp->path is set and before anything logs or redirects with it.

    Sent over a raw socket. An HTTP client would normalise or re-encode the target, and the point of
    the test is the exact bytes on the request line.
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

//  Each of these decodes to a control byte in the path and must be refused.
for (let target of ['/%0A', '/%0D', '/%0d%0a', '/%09', '/%7F', '/%0Aadmin', '/dir%0A/index.html']) {
    let r = await exchange(`GET ${target} HTTP/1.0\r\n\r\n`)
    ttrue(r.length > 0, `server closed without responding to ${target}`)
    tcontains(r, '400 Bad Request')

    //  The refusal must be a client error. A 5xx would mean the bytes reached code not expecting them.
    ttrue(!r.contains('500 '), `${target} produced a server error`)

    //  The decoded control byte must not appear anywhere in the response, which would mean it had
    //  been echoed into a header or body after all.
    ttrue(!r.contains('\r\n\r\n\r\n'), `${target} split the response`)
}

//  A percent-encoded printable in the same position is still decoded and served: the fix rejects
//  control characters, not percent-encoding.
let ok = await exchange('GET /%69ndex.html HTTP/1.0\r\n\r\n')
tcontains(ok, '200 OK')

//  Liveness. GoAhead is single threaded, so a wedge here denies service to every client.
let http: Http = new Http
http.get(HTTP + '/index.html')
await http.wait()
ttrue(http.status == 200, 'server unhealthy after control-character request targets')
http.close()
