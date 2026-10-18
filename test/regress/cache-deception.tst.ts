/* Only static file content is publicly cacheable */

import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'

async function get(path: string, init: any = {}): Promise<Response> {
    let response = await fetch(HTTP + path, { redirect: 'manual', ...init })
    await response.text()
    return response
}

function noStore(response: Response, what: string) {
    let control = response.headers.get('cache-control') || ''
    ttrue(control == 'no-store, private', `${what} is no-store, got "${control}"`)
    ttrue((response.headers.get('vary') || '').includes('Cookie'), `${what} varies on Cookie`)
}

let login = await fetch(HTTP + '/action/sessionTest', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'number=SECRET42',
})
let text = await login.text()
let cookie = (login.headers.get('set-cookie') || '').split(';')[0]
ttrue(cookie.length > 0 && text.includes('SECRET42'), 'the action creates a session')

let tail = await fetch(HTTP + '/action/sessionTest/x.js', { headers: { Cookie: cookie } })
ttrue((await tail.text()).includes('SECRET42'), 'the suffixed URL runs the action with the session')
noStore(tail, 'a session-bound action with a .js suffix')
noStore(await get('/action/sessionTest', { headers: { Cookie: cookie } }), 'a session-bound action')
noStore(await get('/action/sessionTest/x.js'), 'an action with a .js suffix and no session')

let post = await get('/caching/public.js', { method: 'POST', body: 'a=1' })
ttrue(!(post.headers.get('cache-control') || '').includes('public'), 'a POST response is never public')

let file = await get('/caching/public.js')
ttrue(file.status == 200, 'the static file is served')
ttrue(file.headers.get('cache-control') == 'public, max-age=86400', 'a listed static file is still public')
