/*
    sample-escape.tst - The sample handlers escape request data they write into a page
 */

import { Http } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const MARKUP = '<img src=x onerror="alert(1)">'
const ESCAPED = '&lt;img src=x onerror=&quot;alert(1)&quot;&gt;'

let http: Http = new Http
http.get(HTTP + '/action/test?name=' + encodeURIComponent(MARKUP) + '&address=a%26b')
await http.wait()
ttrue(http.status == 200, `the test action must answer, got ${http.status}`)
ttrue(http.response.includes('name: ' + ESCAPED), `the test action must escape the name, got [${http.response}]`)
ttrue(http.response.includes('address: a&amp;b'), `the test action must escape the address, got [${http.response}]`)
ttrue(!http.response.includes('<img'), 'no request markup may reach the page')

http = new Http
http.form(HTTP + '/action/sessionTest', { number: MARKUP })
await http.wait()
ttrue(http.response.includes('Number ' + ESCAPED), `sessionTest must escape the number, got [${http.response}]`)
ttrue(!http.response.includes('<img'), 'no request markup may reach the page')
