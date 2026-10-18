/*
    reroute-authz.tst - A denied request must not have its handler dispatched

    When a handler declines (returns false), websPump reroutes and re-enters websRunRequest. The
    reroute can select a route whose authentication or ability check denies the request and writes
    a 401. websDone sets wp->finalized but leaves wp->state alone, so forcing WEBS_READY afterwards
    dispatched the handler of the very route that had just refused the request.

    The response body is suppressed once finalized, so the disclosure is not always visible on the
    wire. The handler side effects are not suppressed: an anonymous DELETE returned 401 and unlinked
    the file anyway. This test asserts on the side effect, which is what makes the defect observable
    and destructive.

    Route configuration under test (test/route.txt):
        route uri=/reroute/ methods=GET|DELETE handler=upload
        route uri=/reroute/ methods=GET|DELETE auth=basic abilities=manage

    The upload handler is a filter: it declines every request, which is what drives the reroute.
 */

import { Socket, ByteArray, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const httpUri = new Uri(HTTP)

async function request(method: string, uri: string, auth: string = null): Promise<string> {
    let s = new Socket
    s.connect(httpUri.host + ':' + httpUri.port)
    let headers = method + ' ' + uri + ' HTTP/1.0\r\nHost: localhost\r\n'
    if (auth) {
        headers += 'Authorization: Basic ' + auth + '\r\n'
    }
    await s.write(headers + '\r\n')
    let response = new ByteArray(65536)
    while ((await s.read(response, -1)) != null) {}
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? parseInt(match[1]) : 0
}

if (!thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Authentication or upload is not enabled')
} else {
    /*
        joshua:pass1 - the only test user holding the "manage" ability.
     */
    const JOSHUA = 'am9zaHVhOnBhc3Mx'

    /*
        The destructive case. An anonymous DELETE is refused by the ability check on the second route.
        Before the fix the 401 was written and the file handler ran anyway, unlinking the file. The
        status alone does not prove the fix - the file must still be there afterwards.
     */
    let response = await request('PUT', '/reroute/canary.html')
    response = await request('DELETE', '/reroute/canary.html')
    ttrue(status(response) == 401)

    /*
        Confirm the resource was not removed by the refused request. A GET by an authorized user is the
        only way to observe the server's view of the file, so use that rather than the local filesystem.
     */
    response = await request('GET', '/reroute/secret.html', JOSHUA)
    ttrue(status(response) == 200)
    ttrue(response.contains('REROUTE-PROTECTED-BODY'))

    /*
        The disclosure case. Anonymous GET must be refused and must not carry the protected body.
     */
    response = await request('GET', '/reroute/secret.html')
    ttrue(status(response) == 401)
    ttrue(!response.contains('REROUTE-PROTECTED-BODY'))

    /*
        A second anonymous DELETE must still be refused - the first refusal must not have left the
        request in a state that lets a retry through.
     */
    response = await request('DELETE', '/reroute/secret.html')
    ttrue(status(response) == 401)

    response = await request('GET', '/reroute/secret.html', JOSHUA)
    ttrue(status(response) == 200)
    ttrue(response.contains('REROUTE-PROTECTED-BODY'))

    /*
        Authorized requests through the same reroute path must still work, so the fix must not have
        turned the reroute into a blanket denial.
     */
    response = await request('GET', '/reroute/secret.html', JOSHUA)
    ttrue(status(response) == 200)

    /*
        Ordinary routing outside the reroute prefix is unaffected.
     */
    response = await request('GET', '/index.html')
    ttrue(status(response) == 200)
}
