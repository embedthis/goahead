/* A route's abilities=a|b is satisfied by either ability, abilities=a,b needs both */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function status(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    //  mary:pass2
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n' +
                  'Authorization: Basic bWFyeTpwYXNzMg==\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    let match = response.toString().match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

if (thas('ME_GOAHEAD_AUTH')) {
    ttrue(await status('/either/index.html') != '401', 'view must satisfy abilities=view|purchase')
    ttrue(await status('/either-last/index.html') != '401', 'view must satisfy abilities=purchase|view')
    ttrue(await status('/both/index.html') == '401', 'view alone must not satisfy abilities=view,purchase')
}
