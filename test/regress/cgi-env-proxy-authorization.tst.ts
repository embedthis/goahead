/*
    cgi-env-proxy-authorization.tst - Proxy-Authorization must not reach the CGI environment.

    The environment filter in src/cgi.c drops HTTP_AUTHORIZATION so a CGI program never sees the
    request's credentials, which is the same rule Apache and nginx apply. The sibling field
    Proxy-Authorization carries credentials in exactly the same way and was still exported as
    HTTP_PROXY_AUTHORIZATION. A control header on the same request proves the environment dump
    is being read, so an absent variable is a filtered one rather than a failed request.
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { thas, tget, tskip, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function cgi(headers: string): Promise<string> {
    let program = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + program + ' HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nSWITCHES: -e\r\n' +
                  headers + '\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not in this build')
} else {
    let raw = await cgi('Proxy-Authorization: Basic dGVzdDp0ZXN0\r\nX-Env-Probe: present\r\n')
    ttrue(/^HTTP\/1\.1 200 /.test(raw), `the CGI program must run, got [${raw.split('\r\n')[0]}]`)
    ttrue(raw.contains('HTTP_X_ENV_PROBE=present'), 'an ordinary request header is exported as HTTP_*')
    ttrue(!raw.contains('HTTP_PROXY_AUTHORIZATION='), 'Proxy-Authorization credentials must not reach the CGI env')
}
