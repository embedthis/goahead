/*
    Reserve server meta-variable names before storing request parameters for every handler.
 */
import { tget, thas, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const FORGED = 'REMOTE_USER=forged&AUTH_TYPE=basic&REQUEST_METHOD=DELETE&PATH_INFO=/forged&SERVER_NAME=forged.example&' +
    'QUERY_STRING=forged&REMOTE_ADDR=10.9.9.9&plain=kept'

/*
    showTest writes every request variable as one "name=value" line inside <pre>.
*/
function vars(text: string): Map<string, string> {
    let map = new Map<string, string>()
    for (let line of text.split('\n')) {
        let eq = line.indexOf('=')
        if (eq > 0) {
            map.set(line.slice(0, eq), line.slice(eq + 1).replace(/\r$/, ''))
        }
    }
    return map
}

function checkServerValues(v: Map<string, string>, method: string, why: string) {
    ttrue(v.get('REMOTE_USER') === '', `${why}: REMOTE_USER must be the server's (empty) value, got [${v.get('REMOTE_USER')}]`)
    ttrue(v.get('AUTH_TYPE') === '', `${why}: AUTH_TYPE must be the server's (empty) value, got [${v.get('AUTH_TYPE')}]`)
    ttrue(v.get('REQUEST_METHOD') == method, `${why}: REQUEST_METHOD must be ${method}, got [${v.get('REQUEST_METHOD')}]`)
    ttrue(v.get('PATH_INFO') == '/action/showTest', `${why}: PATH_INFO must be the request path, got [${v.get('PATH_INFO')}]`)
    ttrue(v.get('SERVER_NAME') != 'forged.example', `${why}: SERVER_NAME must not be the parameter's value`)
    ttrue(v.get('REMOTE_ADDR') != '10.9.9.9', `${why}: REMOTE_ADDR must not be the parameter's value`)
    ttrue(v.get('QUERY_STRING') != 'forged', `${why}: QUERY_STRING must be the request's query`)
    ttrue(v.get('plain') == 'kept', `${why}: an ordinary parameter must still be stored`)
}

let response = await fetch(`${HTTP}/action/showTest?${FORGED}`)
let text = await response.text()
ttrue(response.status == 200, `the action must run, got ${response.status}`)
checkServerValues(vars(text), 'GET', 'query parameters')

response = await fetch(`${HTTP}/action/showTest`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: FORGED,
})
text = await response.text()
ttrue(response.status == 200, `the action must run, got ${response.status}`)
checkServerValues(vars(text), 'POST', 'form fields')

if (thas('ME_GOAHEAD_AUTH')) {
    //  joshua:pass1 holds manage; the route authenticates, so the server's values name the user
    response = await fetch(`${HTTP}/basicaction/showTest?REMOTE_USER=forged&AUTH_TYPE=digest`, {
        headers: { Authorization: 'Basic am9zaHVhOnBhc3Mx' },
    })
    text = await response.text()
    ttrue(response.status == 200, `the authenticated action must run, got ${response.status}`)
    let v = vars(text)
    ttrue(v.get('REMOTE_USER') == 'joshua', `REMOTE_USER must be the authenticated user, got [${v.get('REMOTE_USER')}]`)
    ttrue(v.get('AUTH_TYPE') == 'basic', `AUTH_TYPE must be the route's scheme, got [${v.get('AUTH_TYPE')}]`)
}
