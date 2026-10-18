/*
    Missing CGI programs must not leak path allocations; measure repeated long-name requests where resident memory
    can be sampled.
 */

import { Cmd, Config, Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

/* Use a sub-255-byte name whose per-request allocation would exceed the growth allowance across the campaign. */
const NAME_LEN = 200
const WARMUP = 200
const REQUESTS = 12000
const MAX_GROWTH_KB = 1024

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

/* Include all Digest fields but an invalid nonce so rejection exercises cloned-username ownership. */
function sanitized(): boolean {
    return tget('ME_SANITIZE') == '1' || tget('ASAN_OPTIONS') != null || tget('UBSAN_OPTIONS') != null
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI not enabled')
} else if (sanitized()) {
    tskip('Resident set is not a leak signal under a sanitizer build')
} else if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('No resident set sampling on windows')
} else {
    let pid = serverPid()
    if (pid < 0 || serverRss(pid) < 0) {
        tskip('Cannot sample the test server resident set')
    } else {
        let http: Http = new Http
        let missing = HTTP + '/cgi-bin/' + 'm'.repeat(NAME_LEN)

        async function hammer(count: number) {
            for (let i = 0; i < count; i++) {
                http.reset()
                http.get(missing)
                await http.wait()
                ttrue(http.status == 404)
            }
        }

        /*  Settle the allocator before sampling the baseline. */
        await hammer(WARMUP)
        let before = serverRss(pid)

        await hammer(REQUESTS)
        let after = serverRss(pid)

        let growth = after - before
        ttrue(growth < MAX_GROWTH_KB,
            'Missing CGI program path leaked: RSS grew ' + growth + ' KB over ' + REQUESTS +
            ' requests (limit ' + MAX_GROWTH_KB + ' KB)')

        /*  Liveness: the server survived the campaign. */
        http.reset()
        http.get(HTTP + '/index.html')
        await http.wait()
        ttrue(http.status == 200)
        http.close()
    }
}
