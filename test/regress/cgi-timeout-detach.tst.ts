/*
    cgi-timeout-detach.tst - Regression test for the CGI registry outliving its request.

    cgiHandler published the request's Webs pointer, and the request's temp file names, into the
    module-global cgiList and returned. Nothing unregistered that entry when the request died by
    another route. The request timeout is an unconditional connection reaper: after
    ME_GOAHEAD_LIMIT_TIMEOUT seconds of silence it called websFree() with no awareness of an
    outstanding CGI child. The 10ms websCgiPoll then kept reading wp->flags and writing through the
    freed object, and websFree had already released wp->cgiStdin which the poll later unlinked -- a
    second use-after-free on a freed string. The child was never signalled on any teardown path, so
    it was also leaked.

    Reachable by an unauthenticated GET of any CGI that stays silent longer than the request
    timeout. The only thing standing between this and heap corruption was assert(websValid(wp)),
    which compiles out in every release build. In a debug build the server burned 100% CPU in an
    assertion loop and never served another request.

    The test holds a CGI silent past the request timeout and then requires the server to still be
    serving. It runs longer than the request timeout by construction, so it cannot be made quick.

    ME_GOAHEAD_LIMIT_TIMEOUT is a compile-time constant with no runtime setter, so the 60 second
    wait cannot be shortened for the test without rebuilding the server.
 */

import { Http, Uri } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'
const uri = new Uri(HTTP)

/*
    cgitest -t sleeps for the given number of seconds before writing anything. The child must
    outlive the server's 60 second request timeout and then wake while the request would still have
    been registered -- the window between the two is where the freed Webs object was used.

    Nothing may issue a NEW connection between the reap and the wake. A new connection in that
    window reclaims the just-freed Webs block, so the registry's dangling pointer lands on a live
    object again and the corruption goes quiet instead of surfacing.

    The reaped response is read as the server writes it, and the wake is waited out afterwards.
 */
const CGI_SLEEP = 70
/*
    REAP_WAIT bounds how long curl will hold the connection: the server reaps at
    ME_GOAHEAD_LIMIT_TIMEOUT (60s), so allow margin beyond that for a loaded machine.

    WAKE_WAIT is measured from the reap, not from the request. The drain already ran until ~60s, so
    this only covers the remaining CGI_SLEEP - 60 seconds until the child wakes, plus margin.
 */
const REAP_WAIT = 90000
const WAKE_WAIT = 25000

if (thas('ME_GOAHEAD_CGI')) {

    /*
        Not App.sleep: it is a synchronous Bun.sleepSync, which would block the event loop for the
        whole wait.
     */
    function delay(ms: number): Promise<void> {
        return new Promise((resolve) => setTimeout(resolve, ms))
    }

    async function alive(path: string): Promise<boolean> {
        let http: Http = new Http
        http.get(HTTP + path)
        await http.wait()
        let ok = http.status == 200
        http.close()
        return ok
    }

    /*
        curl rather than Socket or Http. Both of those give up on a read after 5 seconds, and this
        request must stay open until the server reaps it at 60. Retrying around the 5s limit does not
        help either: the wrapper's socket reaches EOF after ~15s while the server still holds the
        connection, so the 408 is never seen. A raw BSD socket and curl both receive it at ~59.5s
        against the identical server, so the limitation is in the test's socket layer, not the server.

        One process, one connection, held open across the reap. Nothing else may open a NEW
        connection before the child wakes: that reclaims the just-freed Webs block, so the registry's
        dangling pointer lands on a live object and the corruption goes quiet instead of surfacing.
     */
    const proc = Bun.spawn(
        ['curl', '-s', '-i', '--http1.0', '--max-time', String(REAP_WAIT / 1000),
         '-H', 'SWITCHES: -t ' + CGI_SLEEP,
         'http://' + uri.host + ':' + uri.port + '/cgi-bin/cgitest'],
        {stdout: 'pipe', stderr: 'ignore'})
    const text = await new Response(proc.stdout).text()
    await proc.exited

    /* The reaper must close the request out rather than leave the registry pointing at freed memory */
    ttrue(text.contains('408'))

    /*
        Now wait out the child's wake. Its output is what drove websCgiPoll into the freed object, so
        the liveness checks below are only meaningful once the child has written and exited.
     */
    await delay(WAKE_WAIT)

    /*
        The server must still be serving. The child has woken by now and its output is what drove
        the poll into the freed object; on the unfixed build the server is wedged at 100% CPU here.
     */
    ttrue(await alive('/index.html'))

    /* And CGI must still work -- the registry slot has to have been reclaimed, not corrupted */
    ttrue(await alive('/cgi-bin/cgitest'))

} else {
    tskip('CGI not enabled')
}
