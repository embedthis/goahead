/*
    dos.tst - Test DOS attack vectors
 */

import { Http, Socket, Config } from '@embedthis/ejscript'
import { tdepth, tget, tinfo, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"

/*
    Dangling connection attempts per depth. The depth-0 count stays inside the ordinary run's budget;
    the higher depths are the ones that actually press on the connection table, whose shipped cap is
    derived from FD_SETSIZE and needs on the order of a thousand sockets to reach.

    Clamped rather than indexed directly: tm accepts --depth 10 and beyond, and an out-of-range index
    would yield undefined and a loop that never runs - a test that silently stops testing at the
    depths where it matters most.
 */
const ATTEMPTS = [200, 500, 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000]
const attempts = ATTEMPTS[Math.min(tdepth(), ATTEMPTS.length - 1)]

//  Check server available
let http = new Http
http.get(HTTP + "/index.html")
await http.wait()
ttrue(http.status == 200)
http.close()

if (Config.OS != "windows" && Config.OS != "win32") {
    //  Try to create a bunch of dangling sockets. Should not be able to DOS the server
    // Note: Connecting to wrong port intentionally - these should fail quickly
    let i
    const started = Date.now()
    /*
        Held rather than dropped on the floor. The sockets are the attack, so they must stay open
        across the liveness check below - but they must not stay open after it. Left to the garbage
        collector they occupy the server's connection table for an unbounded time, and every test
        that runs afterwards is answered from a saturated server. That is not hypothetical: it
        starved the soak test of half its requests when the two ran in the same invocation.
     */
    const dangling: Socket[] = []
    for (let i = 0; i < attempts; i++) {
        try {
            let s = new Socket
            s.setTimeout(10) // 10ms timeout
            s.connect("127.0.0.1:18080")
            dangling.push(s)
        } catch {}
    }
    tinfo('  ' + attempts + ' dangling connection attempts at depth ' + tdepth() + ', ' +
          ((Date.now() - started) / 1000).toFixed(1) + 's')
    // Give server time to process the connection attempts
    await new Promise(resolve => setTimeout(resolve, 100))

    //  Check server still operating
    http = new Http
    http.get(HTTP + "/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()

    //  Release the table for whatever runs next
    for (let s of dangling) {
        try {
            s.close()
        } catch {}
    }

    /*
        And the server recovers once the attack stops. Without this the test only shows that the
        server survived while saturated, not that it returns to service - a connection slot that is
        never reclaimed would satisfy every assertion above.
     */
    await new Promise(resolve => setTimeout(resolve, 200))
    http = new Http
    http.get(HTTP + "/index.html")
    await http.wait()
    ttrue(http.status == 200, 'the server must still serve after the dangling sockets are closed')
    http.close()
}
