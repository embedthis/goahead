/* An action protected by its own route cannot be run through another action route */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'

async function post(path: string, cookie: string | null = null): Promise<Http> {
    let http: Http = new Http
    http.followRedirects = false
    if (cookie) {
        http.setCookie(cookie)
    }
    http.form(HTTP + path, { number: '7' })
    await http.wait()
    return http
}

async function login(): Promise<string> {
    let http: Http = new Http
    http.followRedirects = false
    http.form(HTTP + '/action/login', { username: 'joshua', password: 'pass1' })
    await http.wait()
    ttrue(http.status == 302, `login must succeed, got ${http.status}`)
    let cookie = http.header('set-cookie').match(/(-goahead-session-=[^;]*)/)[1]
    http.close()
    return cookie
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    //  The protected action is not reachable through the public action route
    let http = await post('/action/authSessionTest')
    ttrue(http.status == 404, `the protected action must not run through /action, got ${http.status}`)
    http.close()

    //  Nor through another public action route
    http = await post('/postonly/authSessionTest')
    ttrue(http.status == 404, `the protected action must not run through /postonly/, got ${http.status}`)
    http.close()

    //  Its own route still demands a login, and runs it once logged in
    http = await post('/auth/authSessionTest')
    ttrue(http.status == 401 || http.status == 302, `the protected route must demand a login, got ${http.status}`)
    http.close()
    let cookie = await login()
    http = await post('/auth/authSessionTest', cookie)
    ttrue(http.status == 200, `the protected route must run the action once logged in, got ${http.status}`)
    http.close()

    //  A route that names an action runs only that one: /action/login does not run /action/loginX
    http = await post('/postonly/login')
    ttrue(http.status == 404, `the login action must only run through /action/login, got ${http.status}`)
    http.close()

    //  Unnamed actions still run through the public action route
    http = await post('/action/showTest')
    ttrue(http.status == 200, `an action no route names must still run through /action, got ${http.status}`)
    http.close()
}
