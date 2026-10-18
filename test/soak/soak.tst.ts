/*
    soak.tst.ts - A sustained mixed workload, and what the server's resource use does across it

    The suite had no test that ran anything for a duration. Everything scales with request size or
    request count - stress/, security/dos.tst.ts - so nothing observed what happens to memory, file
    descriptors or child processes over time. This produces those metrics.

    WHAT IS AND IS NOT MEASURED. This samples the operating system's view of the process: resident
    memory, open descriptors, child processes. It cannot see inside the allocator, and a clean result
    here is not a statement that the server does not leak. What it catches is the class of defect that
    reaches the OS: a descriptor never closed, a CGI child never reaped, a buffer that grows without
    bound. Those are the ones that take a device down after a week.

    WHY BOUNDED GROWTH AND NOT ZERO. Caches fill, the session table populates, and an allocator's
    high-water mark rises and stays risen. All three are correct and all three make RSS grow, then
    plateau. A zero-growth assertion would be flaky and would eventually be "fixed" by deleting it.
    A ratio is a real threshold: it fails on a leak and passes on a warm-up.

    The raw figures are printed every run, whatever the verdict. A pass that reports nothing is not
    evidence, and evidence is what this test exists to produce.

    GoAhead is single threaded, so the arms below are issued in sequence rather than concurrently.
    That is not a limitation of the test: concurrency is what security/dos.tst.ts and
    regress/connection-limit.tst.ts cover, and what this one measures is duration.
 */

import { tdepth, teq, tget, thas, tinfo, tskip, ttrue } from '@embedthis/testme'
import {execSync} from 'node:child_process'
import {readFileSync} from 'node:fs'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'

/*
    Seconds of workload per depth. Depth 0 has to stay inside the ordinary suite's budget, so it is a
    smoke test of the harness rather than a soak; the real runs are depth 3 and above, invoked by
    hand. The plateau this is looking for takes at least a minute to establish.
 */
const SECONDS = [3, 10, 30, 60, 120, 240, 480, 900, 1800, 3600]
const DURATION = SECONDS[tdepth()] * 1000

/*
    Resident memory may grow by this much across the run before it is called a leak. Generous on
    purpose: the first requests populate caches and the session table, and the allocator's high-water
    mark never comes back down. A real leak under this workload is not subtle - it is linear in
    request count, and at these rates that is orders of magnitude, not 60%.
 */
const RSS_GROWTH_LIMIT = 1.6

//  Descriptors and children must not grow at all beyond a small allowance for pooled connections
const FD_GROWTH_LIMIT = 24
const CHILD_GROWTH_LIMIT = 4

interface Sample {
    rssKb: number
    fds: number
    children: number
}

/*
    The server's pid, read from the file setup.sh writes.

    Not found by scanning ps. setup.sh documents why at length: a name match reaches every
    goahead-test on the machine, including the live server of another worktree or a concurrent run of
    this same suite. Measuring the wrong process would report a server as having died while it was
    answering requests normally.
 */
function serverPid(): number {
    try {
        return parseInt(readFileSync(new URL('../.goahead-test.pid', import.meta.url), 'utf8').trim(), 10) || 0
    } catch {
        return 0
    }
}

function sample(pid: number): Sample {
    let rssKb = 0
    let fds = 0
    let children = 0
    try {
        rssKb = parseInt(execSync(`ps -o rss= -p ${pid}`, {encoding: 'utf8'}).trim(), 10) || 0
    } catch {}
    try {
        //  lsof is slow but portable across macOS and Linux; /proc/<pid>/fd is Linux only
        fds = parseInt(execSync(`lsof -p ${pid} 2>/dev/null | wc -l`, {encoding: 'utf8'}).trim(), 10) || 0
    } catch {}
    try {
        children = parseInt(execSync(`pgrep -P ${pid} 2>/dev/null | wc -l`, {encoding: 'utf8'}).trim(), 10) || 0
    } catch {}
    return {rssKb, fds, children}
}

function show(label: string, s: Sample): void {
    tinfo('  ' + label.padEnd(8) + ' rss ' + (s.rssKb / 1024).toFixed(1) + ' MB, ' +
          s.fds + ' descriptors, ' + s.children + ' children')
}

//  One request. Returns true if it answered as expected.
async function get(path: string, expect = 200): Promise<boolean> {
    try {
        let reply = await fetch(HTTP + path)
        //  Draining the body is what makes this a completed request rather than an abandoned one
        await reply.arrayBuffer()
        return reply.status == expect
    } catch {
        return false
    }
}

/*
    An upload through the multipart filter. The filter stages a temp file per part and unlinks it in
    the destructor, so a staging path that leaks shows up here as descriptor or disk growth rather
    than as a failed request.
 */
async function postUpload(): Promise<boolean> {
    const boundary = '----soak-boundary'
    const body = '--' + boundary + '\r\n' +
        'Content-Disposition: form-data; name="myfile"; filename="soak.dat"\r\n' +
        'Content-Type: text/plain\r\n\r\n' +
        'x'.repeat(4096) + '\r\n' +
        '--' + boundary + '--\r\n'
    try {
        let reply = await fetch(HTTP + '/action/uploadTest', {
            method: 'POST',
            headers: {'Content-Type': 'multipart/form-data; boundary=' + boundary},
            body,
        })
        await reply.arrayBuffer()
        return reply.status == 200
    } catch {
        return false
    }
}

/*
    A Basic-auth request. Authentication allocates and frees a username and an abilities table per
    request, and the 401 path frees a different set than the 200 path, so both are worth issuing.
 */
async function authGet(user: string, password: string, expect: number): Promise<boolean> {
    try {
        let reply = await fetch(HTTP + '/auth/basic/basic.html', {
            headers: {Authorization: 'Basic ' + btoa(user + ':' + password)},
        })
        await reply.arrayBuffer()
        return reply.status == expect
    } catch {
        return false
    }
}

const pid = serverPid()

/*
    Whether the process can be sampled at all, decided before any workload is issued. ps, lsof and
    pgrep are absent on Windows and refused under a restrictive sandbox, and in both cases every
    sample reads zero. Asserting on those zeros would report a leak-free server on a machine that
    measured nothing, or - as written first - fail the test for a reason that has nothing to do with
    the server. Skip instead, and say which of the two it was.
 */
const measurable = pid > 0 && process.platform != 'win32' && sample(pid).rssKb > 0

if (!pid) {
    tskip('soak skipped: cannot find the goahead-test process to measure')
} else if (process.platform == 'win32') {
    tskip('soak skipped: ps, lsof and pgrep are not available on Windows')
} else if (!measurable) {
    tskip('soak skipped: cannot read the resident set of pid ' + pid + ' (ps unavailable or refused)')
} else {

    //  Warm up before the baseline, so caches and the session table are populated on both sides of
    //  the measurement and their filling is not counted as growth
    for (let i = 0; i < 20; i++) {
        await get('/index.html')
        await get('/big.txt')
    }

    const before = sample(pid)
    show('start', before)
    ttrue(before.rssKb > 0, 'the server process must be measurable')

    let issued = 0
    let failed = 0
    let rounds = 0
    /*
        Per-arm failure counts. A bare total says the workload broke; it does not say which subsystem
        broke, and with seven arms that is the difference between a diagnosis and a re-run.
     */
    const ARMS = ['static', 'large', 'cgi', 'jst', 'upload', 'auth-ok', 'auth-401', 'notfound']
    //  An arm whose feature the build compiles out is not issued, and a build without authentication
    //  serves the protected page to anyone
    const CGI = thas('ME_GOAHEAD_CGI'), JST = thas('ME_GOAHEAD_JAVASCRIPT'), UPLOAD = thas('ME_GOAHEAD_UPLOAD')
    const REFUSED = thas('ME_GOAHEAD_AUTH') ? 401 : 200
    const armFailures = ARMS.map(() => 0)
    const started = Date.now()

    /*
        The mix. Every arm reaches a different subsystem: a static file, a large static file, a CGI
        fork, a JST page compile and run, an upload through the multipart filter, an authenticated
        request, a rejected authentication, and the error path. A soak that only fetched index.html
        would prove the accept loop does not leak and nothing else.
     */
    while (Date.now() - started < DURATION) {
        let results = [
            await get('/index.html'),
            await get('/big.txt'),
            !CGI || await get('/cgi-bin/cgitest'),
            !JST || await get('/test.jst'),
            !UPLOAD || await postUpload(),
            await authGet('joshua', 'pass1', 200),
            await authGet('joshua', 'wrongpass', REFUSED),
            await get('/no-such-page.html', 404),
        ]
        issued += results.length
        for (let i = 0; i < results.length; i++) {
            if (!results[i]) {
                armFailures[i]++
                failed++
            }
        }
        rounds++
    }

    const after = sample(pid)
    show('end', after)
    tinfo('  ' + rounds + ' rounds, ' + issued + ' requests, ' + failed + ' failed, over ' +
          ((Date.now() - started) / 1000).toFixed(1) + 's at depth ' + tdepth())

    //  Every request answered
    if (failed != 0) {
        console.log('soak: ' + failed + ' of ' + issued + ' requests did not answer as expected')
        console.log('soak: failures by arm -- ' +
            ARMS.map((name, i) => name + ' ' + armFailures[i] + '/' + rounds).join(', '))
    }
    teq(failed, 0)
    ttrue(issued > 0)

    /*
        Reported before asserted, so a failing run leaves the numbers behind rather than only a
        verdict.
     */
    const rssRatio = after.rssKb / before.rssKb
    const fdGrowth = after.fds - before.fds
    const childGrowth = after.children - before.children
    tinfo('  rss x' + rssRatio.toFixed(2) + ', descriptors +' + fdGrowth + ', children +' + childGrowth)

    if (rssRatio > RSS_GROWTH_LIMIT) {
        console.log('soak: resident memory grew ' + rssRatio.toFixed(2) + 'x over ' + issued +
            ' requests (' + before.rssKb + 'KB to ' + after.rssKb + 'KB)')
    }
    ttrue(rssRatio <= RSS_GROWTH_LIMIT)

    if (fdGrowth > FD_GROWTH_LIMIT) {
        console.log('soak: open descriptors grew by ' + fdGrowth + ' over ' + issued + ' requests')
    }
    ttrue(fdGrowth <= FD_GROWTH_LIMIT)

    /*
        Children are CGI processes. GoAhead forks one per CGI request and reaps it when it exits, so
        a count that tracks the request count is a reaping defect.
     */
    if (childGrowth > CHILD_GROWTH_LIMIT) {
        console.log('soak: child processes grew by ' + childGrowth + ' over ' + issued + ' requests')
    }
    ttrue(childGrowth <= CHILD_GROWTH_LIMIT)

    /*
        And the server still serves. A soak that exhausted a pool or wedged the single-threaded event
        loop would still satisfy every growth bound above - the counters would simply stop moving.
     */
    ttrue(await get('/index.html'), 'the server must still answer after the soak')
    ttrue(!CGI || await get('/cgi-bin/cgitest'), 'CGI must still run after the soak')
    ttrue(await authGet('joshua', 'pass1', 200), 'authentication must still succeed after the soak')
}
