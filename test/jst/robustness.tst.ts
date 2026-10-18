/*
    robustness.tst - Limits and resource behaviour of the template engine
 */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, tcontains } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

async function fetch(path: string): Promise<Http> {
    let http: Http = new Http
    http.setLimits({requestTimeout: 5})
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
    ttrue(http.status == 500)
    tcontains(http.response, 'Internal Server Error')
    await alive()

    http = await fetch('/jst/deep.jst')
    ttrue(http.status == 500)
    tcontains(http.response, 'Internal Server Error')
    await alive()

    /*
        Invalid statement openers in bare blocks must fail instead of repeating a put-back token.
        Include every token class and a nested block; each error must leave the event loop serving.
     */
    const badBlocks = ['blockexpr', 'blockmul', 'blockcomma', 'blocklogical', 'blockassign',
        'blockincdec', 'blockelse', 'blocknested']
    for (const name of badBlocks) {
        http = await fetch(`/jst/${name}.jst`)
        ttrue(http.status == 500)
        tcontains(http.response, 'Internal Server Error')
        await alive()
    }
    http = await fetch('/jst/blockok.jst')
    ttrue(http.status == 200)
    ttrue(http.response.trim() == 'ok')
    await alive()

    //  A run of failing templates leaves the server responsive
    for (let i = 0; i < 20; i++) {
        http = await fetch('/jst/deep.jst')
        ttrue(http.status == 500)
        http = await fetch('/jst/long.jst')
        ttrue(http.status == 500)
        http = await fetch('/jst/unterm.jst')
        ttrue(http.status == 200)
        for (const name of badBlocks) {
            http = await fetch(`/jst/${name}.jst`)
            ttrue(http.status == 500)
            tcontains(http.response, 'Internal Server Error')
            await alive()
        }
    }
    await alive()
}
