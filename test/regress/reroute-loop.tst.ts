/*
    reroute-loop.tst - A resumed route scan that runs off the end must fail, not wrap to the start

    When a handler declines (returns false), websPump calls websRouteRequest again and that scan
    resumes after the route that just declined. If the resumed index ran off the end of the route
    table it wrapped back to index zero, so the same route matched again, declined again, and the
    pump spun. The WEBS_MAX_ROUTE limiter could not stop it because wp->routeCount was only advanced
    on the WEBS_REROUTE flag path, which this loop never takes.

    The result was a denial of service from one unauthenticated request: GoAhead is single threaded,
    so the spinning request pinned the event loop and no further connection was serviced. With trace
    enabled it was also an unbounded disk fill.

    Route configuration under test (test/route.txt), last in the table:
        route uri=/reroute-loop/ methods=DELETE handler=upload

    The upload handler is a filter that declines every request. DELETE is used because the
    unauthenticated catch-all "/" accepts only the safe methods, so it cannot match first and end the
    scan - which leaves the declining route as the last matching route for the request. That is the
    case where the resumed scan runs off the end.

    A wedged server does not hang this test: the Socket read path gives up after a few seconds, so
    the response comes back empty and the status assertion fails.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const httpUri = new Uri(HTTP)

async function request(method: string, uri: string): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    await s.write(method + ' ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n\r\n')
    let response = new ByteArray(65536)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

/*
    The defect itself. A DELETE whose only matching route declines must be answered. Before the fix
    no response was ever written and the server never returned to the event loop.
 */
let response = await request('DELETE', '/reroute-loop/x.html')
ttrue(status(response) == 404)

/*
    The server must still be alive. This is the assertion that separates "the request failed" from
    "the request took the whole server down with it", which is the actual security impact.
 */
response = await request('GET', '/index.html')
ttrue(status(response) == 200)

/*
    Repeat the trigger. The loop limiter lives in per-request state, so a second attempt against the
    same server must behave identically - a counter that survived the first request would either
    poison later requests or stop counting.
 */
response = await request('DELETE', '/reroute-loop/y.html')
ttrue(status(response) == 404)
response = await request('DELETE', '/reroute-loop/z.html')
ttrue(status(response) == 404)

response = await request('GET', '/index.html')
ttrue(status(response) == 200)

/*
    The fix must not have broken the legitimate decline-and-continue path. The upload handler on
    /reroute/ declines and routing continues to the route behind it, which authenticates and denies.
    A 401 here proves a resumed scan still reaches the routes that follow the decliner.
 */
response = await request('GET', '/reroute/secret.html')
ttrue(status(response) == 401)

/*
    Ordinary routing is unaffected.
 */
response = await request('GET', '/index.html')
ttrue(status(response) == 200)
