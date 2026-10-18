/*
    absolute-target-host.tst - An absolute-form target supplies the authority, not the Host header
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function exchange(payload: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(payload)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function resolvedHost(raw: string): string {
    let match = raw.match(/host=([^\r\n]*)/)
    return match ? match[1] : ''
}

function hostGet(target: string, host: string): string {
    return 'GET ' + target + ' HTTP/1.1\r\nHost: ' + host + '\r\nConnection: close\r\n\r\n'
}

let raw = await exchange(hostGet('http://target.example/action/hostTest', 'attacker.example'))
ttrue(raw.contains('200 OK'), 'the absolute-form request must be served')
ttrue(resolvedHost(raw) == 'target.example',
      `the target's authority must win over Host, got [${resolvedHost(raw)}]`)

raw = await exchange(hostGet('/action/hostTest', 'plain.example'))
ttrue(raw.contains('200 OK'), 'an origin-form request must be served')
ttrue(resolvedHost(raw) == 'plain.example',
      `Host must still identify the site for an origin-form target, got [${resolvedHost(raw)}]`)

raw = await exchange('GET http://target.example/index.html HTTP/1.1\r\n' +
                     'Host: bad host\r\nConnection: close\r\n\r\n')
ttrue(!raw.contains('200 OK'), 'a malformed Host must still be rejected')
