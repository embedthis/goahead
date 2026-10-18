/*
    escape.tst - Templates can write request data safely
 */

import { Http } from '@embedthis/ejscript'
import { tcontains, tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

async function fetch(path: string): Promise<Http> {
    let http: Http = new Http
    http.get(HTTP + path)
    await http.wait()
    return http
}

if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JavaScript templates not enabled')
} else {
    let value = `<script>alert("x")</script>&'`
    let http = await fetch('/jst/escape.jst?q=' + encodeURIComponent(value))
    ttrue(http.status == 200, `the template must run, got ${http.status}`)
    let parts = http.response.trim().split('|')
    ttrue(parts.length == 5, `expected five outputs, got [${http.response}]`)

    let html = '&lt;script&gt;alert(&quot;x&quot;)&lt;/script&gt;&amp;&#39;'
    ttrue(parts[0] == html, `writeHtml must escape markup, quotes and ampersands, got [${parts[0]}]`)
    ttrue(parts[1] == html, `escape must return the same escaped text, got [${parts[1]}]`)
    ttrue(!parts[2].match(/[<>" ]/), `escapeUri must encode markup, double quotes and spaces, got [${parts[2]}]`)
    ttrue(decodeURI(parts[2]) == value, `escapeUri must decode back to the value, got [${parts[2]}]`)
    ttrue(parts[3] == html + ' &amp;', `writeHtml must escape each argument and join them with a space, got [${parts[3]}]`)
    ttrue(!http.response.includes('<script>'), 'no request markup may reach the page')
    ttrue(!parts[4].match(/[<>"'&]/), `escapeJs must encode markup, quotes and ampersands, got [${parts[4]}]`)


    let nasty = `\\ ' " < </script> \r\n \u2028 \u2029 & / = \` ok,._-é`
    http = await fetch('/jst/escape.jst?q=' + encodeURIComponent(nasty))
    ttrue(http.status == 200, `the template must run, got ${http.status}`)
    let js = http.response.trim().split('|')[4]
    ttrue(!js.match(/['"<>&\/=`\r\n\u2028\u2029]/), `escapeJs must leave no raw special character, got [${js}]`)
    ttrue(!js.match(/\\[^u]/), `escapeJs must only use \\u escapes, got [${js}]`)
    ttrue(JSON.parse('"' + js + '"') == nasty, `escapeJs must round-trip through a string literal, got [${js}]`)
    ttrue(js.includes('\\u2028'), `escapeJs must escape U+2028, got [${js}]`)
    ttrue(js.includes('é') && js.includes('ok,._-'), `escapeJs must pass safe characters through, got [${js}]`)

    //  A script, handler and quoted attribute written with escapeJs and writeHtml hold no injection
    http = await fetch('/jst/jsctx.jst?first=%5C&last=;alert(1)//&item=%27);alert(2);//&q=x%20onfocus=alert(3)%20autofocus')
    ttrue(http.status == 200, `the template must run, got ${http.status}`)
    tcontains(http.response, "var first = '\\u005C'; var last = '\\u003Balert\\u00281\\u0029\\u002F\\u002F';")
    tcontains(http.response, "select('\\u0027\\u0029\\u003Balert\\u00282\\u0029\\u003B\\u002F\\u002F')")
    tcontains(http.response, 'value="x onfocus=alert(3) autofocus"')

    http = await fetch('/jst/query.jst?q=' + encodeURIComponent('<b>'))
    ttrue(http.response.includes('q=[<b>]'), `write must still write raw, got [${http.response}]`)

    http = await fetch('/jst/escapeargs.jst')
    ttrue(http.status == 500, `escape() with two arguments must fail the page, got ${http.status}`)
}
