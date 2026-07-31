/*
    digest-username.tst - Regression test for the Digest username leak.

    parseDigestDetails (src/auth.c) takes ownership of wp->username via wfree()+sclone()
    before it can fail. websAuthenticate's parseAuth failure branch used to clear that
    pointer with `wp->username = 0;` instead of freeing it, orphaning an attacker-sized
    block (bounded only by ME_GOAHEAD_LIMIT_HEADER) on every failed Digest attempt.
    Request teardown then saw NULL and reclaimed nothing.

    This drives a large number of pre-auth Digest failures with a maximal username and
    watches the server's resident set. Pre-fix the leak is ~1.5 KB per request, so the
    campaign below grows the heap by several megabytes. Post-fix growth is allocator
    noise.

    Skipped where the resident set cannot be sampled (Windows, or no usable ps/pgrep).
 */

import { Cmd, Config, Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

/*
    Sized so the pre-fix leak (REQUESTS * ~USERNAME_LEN bytes) is an order of magnitude
    above allocator noise, while the campaign still runs in a few seconds.
 */
const USERNAME_LEN = 1500
const WARMUP = 200
const REQUESTS = 3000
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

/*
    A Digest header that parses far enough for parseDigestDetails to clone the username,
    then fails on the nonce. Every field is present so the failure is the nonce check,
    not an early missing-field return -- that keeps the test pinned to the ownership
    transfer rather than to any one rejection path.
 */
function digestHeader(): string {
    return 'Digest username="' + 'a'.repeat(USERNAME_LEN) + '", realm="example.com", ' +
        'nonce="!!!not-base64!!!", response="x", uri="/auth/digest/digest.html", ' +
        'qop="auth", nc="00000001", cnonce="c"'
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication not enabled')
} else if (Config.OS == 'windows') {
    tskip('No resident set sampling on windows')
} else {
    let pid = serverPid()
    if (pid < 0 || serverRss(pid) < 0) {
        tskip('Cannot sample the test server resident set')
    } else {
        let http: Http = new Http

        async function hammer(count: number) {
            for (let i = 0; i < count; i++) {
                http.reset()
                http.setHeader('Authorization', digestHeader())
                http.get(HTTP + '/auth/digest/digest.html')
                await http.wait()
                ttrue(http.status == 401)
            }
        }

        /*  Settle the allocator before sampling the baseline. */
        await hammer(WARMUP)
        let before = serverRss(pid)

        await hammer(REQUESTS)
        let after = serverRss(pid)

        let growth = after - before
        ttrue(growth < MAX_GROWTH_KB,
            'Digest failure path leaked: RSS grew ' + growth + ' KB over ' + REQUESTS +
            ' failed Digest requests (limit ' + MAX_GROWTH_KB + ' KB)')

        /*  Liveness: the server survived the campaign. */
        http.reset()
        http.get(HTTP + '/index.html')
        await http.wait()
        ttrue(http.status == 200)
    }
}
