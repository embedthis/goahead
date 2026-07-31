/*
    robustness.tst - Limits and resource behaviour of the template engine

    GoAhead is single threaded, so a template that wedges or crashes the interpreter takes the whole
    server with it. These tests drive the engine past its documented limits (JS_MAX_TOKEN,
    JS_MAX_RECURSE) and over large pages, and check after each that the server is still serving.
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, tcontains } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

async function fetch(path: string): Promise<Http> {
    let http: Http = new Http
    http.get(HTTP + path)
    await http.wait()
    return http
}

async function alive() {
    let http = await fetch('/index.html')
    ttrue(http.status == 200)
}

if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else {
    //  A large generated page is served in full
    let http = await fetch('/big.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Line: 0 ')
    tcontains(http.response, 'Line: 799 ')
    await alive()

    //  200 script blocks in one page, all evaluated in order in the one engine
    http = await fetch('/jst/many.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'S' + Array.from({length: 200}, (_, i) => i).join('') + 'E')
    await alive()

    //  A string literal within JS_MAX_TOKEN (1024) is fine
    http = await fetch('/jst/longok.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'ok:' + 'A'.repeat(900))
    await alive()

    //  A literal past JS_MAX_TOKEN is rejected by the lexer, not truncated or overflowed
    http = await fetch('/jst/long.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Token too big')
    await alive()

    //  Deeply nested parentheses hit the JS_MAX_RECURSE guard rather than exhausting the C stack
    http = await fetch('/jst/deep.jst')
    ttrue(http.status == 200)
    tcontains(http.response, 'Script recursion too deep')
    await alive()

    //  A run of failing templates leaves the server responsive
    for (let i = 0; i < 20; i++) {
        http = await fetch('/jst/deep.jst')
        ttrue(http.status == 200)
        http = await fetch('/jst/long.jst')
        ttrue(http.status == 200)
        http = await fetch('/jst/unterm.jst')
        ttrue(http.status == 200)
    }
    await alive()
}
