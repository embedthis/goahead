/* A query or form parameter cannot create, replace or extend a header variable */
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const BOUNDARY = 'HeaderShadow'

if (thas('ME_GOAHEAD_CGI')) {
    let response = await fetch(`${HTTP}/cgi-bin/cgitest?HTTP_ORIGIN=1&HTTP_SEC_FETCH_SITE=1`, {
        headers: { 'Origin': 'http://evil.example', 'Sec-Fetch-Site': 'cross-site' },
    })
    let text = await response.text()
    ttrue(response.status == 200, `the CGI program must run, got ${response.status}`)
    ttrue(text.includes('HTTP_ORIGIN=http://evil.example<'), 'HTTP_ORIGIN must hold only the header value')
    ttrue(text.includes('HTTP_SEC_FETCH_SITE=cross-site<'), 'HTTP_SEC_FETCH_SITE must hold only the header value')
    ttrue(!text.includes('CGI_HTTP_ORIGIN'), 'the header must not be demoted to an untrusted variable')
} else {
    tskip('CGI is not in this build')
}

let response = await fetch(`${HTTP}/action/showTest?HTTP_X_REQUESTED_WITH=XMLHttpRequest`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'HTTP_SEC_FETCH_SITE=same-origin&plain=kept',
})
let text = await response.text()
ttrue(response.status == 200, `the action must run, got ${response.status}`)
ttrue(!text.includes('HTTP_X_REQUESTED_WITH'), 'a query parameter must not define a header variable')
ttrue(!text.includes('HTTP_SEC_FETCH_SITE'), 'a form field must not define a header variable')
ttrue(text.includes('plain'), 'an ordinary field must still be defined')

if (thas('ME_GOAHEAD_UPLOAD')) {
    let body = `--${BOUNDARY}\r\nContent-Disposition: form-data; name="HTTP_USER_AGENT"\r\n\r\nforged\r\n--${BOUNDARY}--\r\n`
    response = await fetch(`${HTTP}/action/uploadTest`, {
        method: 'POST',
        headers: { 'Content-Type': `multipart/form-data; boundary=${BOUNDARY}`, 'User-Agent': 'real-agent' },
        body,
    })
    text = await response.text()
    ttrue(response.status == 200, `the upload must be accepted, got ${response.status}`)
    ttrue(text.includes('HTTP_USER_AGENT=real-agent\r\n'), 'a multipart field must not replace a header variable')
    ttrue(!text.includes('forged'), 'the multipart value must not be stored under the header name')
}
