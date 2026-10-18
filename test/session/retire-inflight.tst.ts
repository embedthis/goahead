/*
    retire-inflight.tst - A logout must not free a session another request is still holding.

    Routing runs at the end of parseIncoming, before the request body is read, so websAuthenticate
    resolves and caches the session on the request while it parks in WEBS_CONTENT waiting for its
    body. A logout on a second connection used to call freeSession() on that object and clear only
    the logging-out request's pointer, leaving the parked request holding freed memory. Finishing the
    body then read and wrote through the dangling pointer.

    Sessions are now retired rather than freed: removed from the session store immediately, freed
    only when the last holder releases it in termWebs. A parked request completes against a session
    that is still valid memory, and the logout still takes effect for every new request.

    Under a plain build a use-after-free is silent, so this drives the exact interleaving and asserts
    the parked requests still complete correctly and the server survives. Under ASan the same
    interleaving aborts pre-fix.

    Note: the delay between parking a request and logging it out must yield to the event loop, or the
    parked bytes never leave the client and the server routes the request after the logout - which
    tests nothing. App.sleep blocks, so use a timer.
 */

import { ByteArray, Http, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || "127.0.0.1:8080"
const uri = new Uri(HTTP)
const ADDR = uri.host + ":" + uri.port
const DELAY = 150
const BODY = "number=1234567"

function delay(): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, DELAY))
}

async function login(): Promise<string> {
    let http: Http = new Http
    http.form(HTTP + "/action/login", {username: "joshua", password: "pass1"})
    await http.wait()
    ttrue(http.status == 302)
    let cookie = http.header("set-cookie").match(/(-goahead-session-=[^;]*)/)[1]
    http.close()
    return cookie
}

async function sessionCount(): Promise<number> {
    let http: Http = new Http
    http.get(HTTP + "/action/sessions")
    await http.wait()
    ttrue(http.status == 200, `the sessions action must answer 200, got ${http.status}`)
    let body = http.response
    // Parse signed counts to detect accounting underflow.
    let match = body.match(/sessions=(-?\d+)/)
    ttrue(match != null, `the sessions action must report a count, got [${body}]`)
    http.close()
    return match ? parseInt(match[1]) : -1
}

async function logout(cookie: string) {
    let http: Http = new Http
    http.setCookie(cookie)
    http.post(HTTP + "/action/logout")
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
    http.close()
}

/*
    Send an authenticated request's headers with a Content-Length whose body is withheld. The server
    routes and authenticates it - caching the session on the request - then parks waiting for content.
 */
async function park(cookie: string, body: string = BODY): Promise<Socket> {
    let s = new Socket
    s.connect(ADDR)
    await s.write("POST /auth/authSessionTest HTTP/1.1\r\nHost: " + uri.host + "\r\nCookie: " + cookie +
        "\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: " + body.length +
        "\r\nConnection: close\r\n\r\n")
    await delay()
    parkedBodies.set(s, body)
    return s
}

const parkedBodies = new Map<Socket, string>()

async function finish(s: Socket): Promise<string> {
    let response = new ByteArray(65536, true)
    let n
    await s.write(parkedBodies.get(s) ?? BODY)
    parkedBodies.delete(s)
    try {
        while ((n = await s.read(response, -1)) != null) {}
    } catch {
        /* Read timeout: treat as end of exchange */
    }
    s.close()
    return response.toString()
}

if (thas('ME_GOAHEAD_AUTH')) {
    let http: Http = new Http

    /*
        One holder. Retire the session from under a parked request, then let its body arrive.
     */
    let cookie = await login()
    let parked = await park(cookie)
    await logout(cookie)
    ttrue((await finish(parked)).contains('HTTP/1.1 200 OK'))

    //  The logout took effect: the retired session is unreachable by any new request.
    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 302)
    ttrue(new Uri(http.header('location')).path == '/auth/form/login.html')
    http.close()

    /*
        Several holders of the one session. Only the last release may free it.
     */
    cookie = await login()
    let held: Socket[] = []
    for (let i = 0; i < 4; i++) {
        held.push(await park(cookie))
    }
    await logout(cookie)
    for (let s of held) {
        ttrue((await finish(s)).contains('HTTP/1.1 200 OK'))
    }

    /*
        Repeat the race so a stale pointer would be reused by a later allocation.
     */
    for (let i = 0; i < 10; i++) {
        cookie = await login()
        parked = await park(cookie)
        await logout(cookie)
        ttrue((await finish(parked)).contains('HTTP/1.1 200 OK'))
    }

    //  The server is still alive and serving after every one of those retirements.
    http.reset()
    http.get(HTTP + "/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()

    //  And a fresh login still gets a working session.
    cookie = await login()
    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()
    await logout(cookie)

    /*
        Retire one session from two requests and verify the count decreases only once.
     */
    const CYCLES = 8
    let baseline = await sessionCount()
    for (let i = 0; i < CYCLES; i++) {
        cookie = await login()
        parked = await park(cookie, "number=1&destroy=1")
        await logout(cookie)
        ttrue((await finish(parked)).contains('HTTP/1.1 200 OK'))
    }
    let after = await sessionCount()
    ttrue(after == baseline, `session count drifted from ${baseline} to ${after} over ${CYCLES} double-retire cycles`)

    cookie = await login()
    http.reset()
    http.setCookie(cookie)
    http.get(HTTP + "/auth/form/index.html")
    await http.wait()
    ttrue(http.status == 200)
    http.close()
    await logout(cookie)
    ttrue((await sessionCount()) == baseline, 'a normal login and logout must return the count to baseline')
}
