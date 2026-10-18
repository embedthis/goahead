/* Resource-limit errors must return control to the HTTP event loop. */
import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue, tcontains } from '@embedthis/testme'
const HTTP = tget('TM_HTTP')
/*
    Allow sanitizer overhead; this test checks bounded execution, not speed.
 */
const SANITIZED = tget('ME_SANITIZE', '') == '1' || !!process.env.ASAN_OPTIONS || !!process.env.UBSAN_OPTIONS
const TIMEOUT = SANITIZED ? 60 : 5
async function fetch(path: string): Promise<Http> {
    const http = new Http
    http.setLimits({requestTimeout: TIMEOUT})
    http.get(HTTP + path)
    await http.wait()
    return http
}
if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else {
    for (const [path, message] of [
        ['budgetloop.jst?n=1000000000', 'Script step limit exceeded'],
        ['budgetforever.jst', 'Script step limit exceeded'],
        ['budgetstring.jst', 'Script result limit exceeded'],
    ]) {
        let http = await fetch('/jst/' + path)
        ttrue(http.status == 500)
        tcontains(http.response, 'Internal Server Error')
        ttrue(!http.response.includes(message))
        http = await fetch('/index.html')
        ttrue(http.status == 200)
        http = await fetch('/jst/budgetloop.jst?n=10')
        ttrue(http.status == 200)
        ttrue(http.response.trim() == 'done')
    }
}
