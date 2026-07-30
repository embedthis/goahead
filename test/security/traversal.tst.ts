/*
    traversal.tst - Directory traversal must never disclose a file outside the web root.

    Security-scan-framework T-4 (audit finding 1.1#1): the previous assertions here were
    `ttrue(http.status >= 200)`, which is true for EVERY valid HTTP response (200/400/404
    all pass) — the test could not fail even if the server served the target file. These
    assertions instead check the response BODY does not contain the out-of-root file's
    content, which is the actual security invariant, and confirm escape attempts are
    rejected rather than served.
 */

import { Http } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

/*
    Request `path` and assert the response never contains any of `leaks` (markers that
    would only appear if a file above the web root had actually been served). `escapes`
    means the path resolves outside the root and MUST be rejected (4xx), not served (200).
 */
async function mustNotDisclose(path: string, leaks: string[], escapes: boolean) {
    const http = new Http
    http.get(HTTP + path)
    await http.wait()
    const body = http.response || ""
    for (const marker of leaks) {
        ttrue(!body.contains(marker), `traversal ${path} must not disclose "${marker}"`)
    }
    if (escapes) {
        //  An out-of-root escape must be rejected, never answered with a 200 body.
        ttrue(http.status >= 400 && http.status < 500,
              `traversal ${path} must be rejected (4xx), got ${http.status}`)
    }
    http.close()
}

//  Password/shadow markers ("root:x:0:0"), and auth-file markers ("password=", "roles=").
await mustNotDisclose("/../auth.conf", ["password=", "roles=", "root:"], true)
await mustNotDisclose("/..%5Cauth.conf", ["password=", "roles=", "root:"], true)   /* Windows '\' delimiter */
await mustNotDisclose("/../../../../../.x/.x/.x/.x/.x/.x/etc/passwd", ["root:", "password="], true)

//  `/../../index.html` normalizes back inside the root; it may legitimately serve the
//  real index (200) or be rejected, but it must never leak an out-of-root file.
await mustNotDisclose("/../../index.html", ["root:", "password="], false)

//  Liveness: the server still serves a legitimate in-root file after the abuse cases.
const http = new Http
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200, "server still serves index.html after traversal probes")
http.close()
