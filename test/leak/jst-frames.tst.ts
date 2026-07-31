/*
    jst-frames.tst - Leak gate for the JST engine.

    Every .jst request builds a page buffer, a JavaScript engine and a script variable frame, and
    tears them all down in jstHandler. The frame is a symbol table that jsCloseEngine does not own,
    so a missing jsCloseBlock leaks one hash table plus every variable in it per request.

    This drives the success path, the shadow-a-request-parameter path and the script-error path with
    a large value and watches the server's resident set. A leaked frame is ~1 KB per request, so the
    campaign below would grow the heap by megabytes.

    Skipped where the resident set cannot be sampled (Windows, or no usable ps/pgrep).
 */

import { Cmd, Config, Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

/*  Fits inside ME_GOAHEAD_LIMIT_HEADER (2048) along with the rest of the request line. */
const FILLER = 'A'.repeat(1200)
const WARMUP = 100
const REQUESTS = 1500
const MAX_GROWTH_KB = 1024

/*
    Resident set of the test server in KB, or -1 if it cannot be sampled.
 */
function serverRss(pid: number): number {
    try {
        let cmd = new Cmd('ps -o rss= -p ' + pid)
        if (cmd.status != 0) {
            return -1
        }
        let rss = parseInt(cmd.response.trim())
        return (rss > 0) ? rss : -1
    } catch (e) {
        return -1
    }
}

function serverPid(): number {
    try {
        let cmd = new Cmd('pgrep -f goahead-test')
        if (cmd.status != 0) {
            return -1
        }
        /*  Take the first match. The suite runs one test server. */
        let pid = parseInt(cmd.response.trim().split('\n')[0])
        return (pid > 0) ? pid : -1
    } catch (e) {
        return -1
    }
}

if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else if (Config.OS == 'windows') {
    tskip('No resident set sampling on windows')
} else {
    let pid = serverPid()
    if (pid < 0 || serverRss(pid) < 0) {
        tskip('Cannot sample the test server resident set')
    } else {
        let http: Http = new Http

        /*  One pass over the success, shadow and error exits of jstHandler. */
        async function hammer(count: number) {
            for (let i = 0; i < count; i++) {
                http.reset()
                http.get(HTTP + '/jst/multi.jst')
                await http.wait()
                ttrue(http.status == 200)

                http.reset()
                http.get(HTTP + '/jst/shadow.jst?q=' + FILLER)
                await http.wait()
                ttrue(http.status == 200)

                http.reset()
                http.get(HTTP + '/jst/undef.jst?q=' + FILLER)
                await http.wait()
                ttrue(http.status == 200)
            }
        }

        /*  Settle the allocator before sampling the baseline. */
        await hammer(WARMUP)
        let before = serverRss(pid)

        await hammer(REQUESTS)
        let after = serverRss(pid)

        let growth = after - before
        ttrue(growth < MAX_GROWTH_KB,
            'JST engine leaked: RSS grew ' + growth + ' KB over ' + (REQUESTS * 3) +
            ' template requests (limit ' + MAX_GROWTH_KB + ' KB)')

        /*  Liveness: the server survived the campaign. */
        http.reset()
        http.get(HTTP + '/index.html')
        await http.wait()
        ttrue(http.status == 200)
    }
}
