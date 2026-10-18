/* An authentication gate without methods= applies to every method */

import { Http } from '@embedthis/ejscript'
import { tget, thas, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:8080'

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Authentication is not in this build')
} else {
    let http: Http = new Http

    for (let method of ['PUT', 'DELETE']) {
        http.connect(method, HTTP + '/gated/test')
        await http.wait()
        ttrue(http.status == 401, `${method} through the gate without credentials must get 401, got ${http.status}`)
        http.close()
    }

    http.setCredentials('joshua', 'pass1')
    for (let method of ['PUT', 'DELETE']) {
        http.connect(method, HTTP + '/gated/test')
        await http.wait()
        ttrue(http.status == 200, `${method} through the gate with credentials must reach the action, got ${http.status}`)
        http.close()
    }

    http = new Http
    http.get(HTTP + '/gated/test')
    await http.wait()
    ttrue(http.status == 401, `GET through the gate without credentials must still get 401, got ${http.status}`)
    http.close()


    for (let method of ['HEAD', 'GET']) {
        http = new Http
        http.connect(method, HTTP + '/headgate/sub/x.html')
        await http.wait()
        ttrue(http.status == 401, `anonymous ${method} through a methods= gate must get 401, got ${http.status}`)
        http.close()
    }
    http = new Http
    http.setCredentials('joshua', 'pass1')
    http.connect('HEAD', HTTP + '/headgate/sub/x.html')
    await http.wait()
    ttrue(http.status == 200, `authenticated HEAD through a methods= gate must be served, got ${http.status}`)
    http.close()
}
