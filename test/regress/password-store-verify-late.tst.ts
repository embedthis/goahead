/* A password verifier installed after the routes are loaded applies to them */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'

async function get(path: string, credentials = true): Promise<number> {
    let http: Http = new Http
    if (credentials) {
        http.setCredentials('joshua', 'pass1', 'basic')
    }
    http.get(HTTP + path)
    await http.wait()
    let status = http.status
    http.close()
    return status
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not enabled')
} else {
    ttrue(await get('/auth/basic/basic.html') == 200, 'the store verifier must accept joshua')
    try {
        ttrue(await get('/action/verifierTest?refuse=1', false) == 200, 'install the refusing verifier')
        let status = await get('/auth/basic/basic.html')
        ttrue(status == 401, `a verifier installed after websOpen must apply to loaded routes, got ${status}`)
    } finally {
        await get('/action/verifierTest', false)
    }
    ttrue(await get('/auth/basic/basic.html') == 200, 'the restored store verifier must accept joshua again')
}
