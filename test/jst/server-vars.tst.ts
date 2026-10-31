/*
    Define server variables before request parameters so templates cannot read spoofed values.
 */
import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else {
    let http = new Http
    http.get(HTTP + '/jst/server-vars.jst?REMOTE_ADDR=10.9.9.9&REQUEST_METHOD=DELETE&plain=kept')
    await http.wait()
    ttrue(http.status == 200, `the template must run, got ${http.status}`)
    let text = http.response.trim()
    ttrue(!text.includes('10.9.9.9') && /addr=\[[0-9a-f.:]+\]/.test(text), `REMOTE_ADDR must be the peer, got [${text}]`)
    ttrue(text.includes('method=[GET]'), `REQUEST_METHOD must be the request's method, got [${text}]`)
    ttrue(text.includes('plain=[kept]'), `an ordinary parameter is still a template variable, got [${text}]`)
    http.close()
}
