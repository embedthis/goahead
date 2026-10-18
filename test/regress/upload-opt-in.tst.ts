/*
    upload-opt-in.tst - Multipart uploads are parsed only on routes that accept them
 */
import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tcontains, thas, tget, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

const BODY = '--BOUNDARY\r\n' +
             'Content-Disposition: form-data; name="myfile"; filename="probe.txt"\r\n' +
             'Content-Type: text/plain\r\n' +
             '\r\n' +
             'probe-contents\r\n' +
             '--BOUNDARY--\r\n'

async function post(target: string, extra: string = ''): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('POST ' + target + ' HTTP/1.0\r\nHost: localhost\r\n' + extra +
                  'Content-Type: multipart/form-data; boundary=BOUNDARY\r\n' +
                  'Content-Length: ' + BODY.length + '\r\n\r\n' + BODY)
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(response: string): number {
    let match = response.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? Number(match[1]) : 0
}

if (!thas('ME_GOAHEAD_UPLOAD')) {
    tskip('Uploads are not in this build')
} else {
    let response = await post('/index.html')
    ttrue(status(response) == 415, `a route without upload=true must refuse an upload with 415, got ${status(response)}`)
    tcontains(response.toLowerCase(), 'connection: close', 'and close the connection, as the body is not read')

    response = await post('/action/uploadTest')
    ttrue(status(response) == 200, `a route with upload=true must accept the upload, got ${status(response)}`)
    tcontains(response, 'CLIENT=probe.txt', 'and parse it')

    if (thas('ME_GOAHEAD_CGI')) {
        let program = process.platform == 'win32' ? '/cgi-bin/cgitest.exe' : '/cgi-bin/cgitest'
        response = await post(program, 'SWITCHES: -p\r\n')
        ttrue(status(response) == 200, `a CGI route must accept a multipart body, got ${status(response)}`)
        tcontains(response, 'probe-contents', 'and pass it to the program unparsed')
        tcontains(response, '--BOUNDARY--', 'including its closing boundary')
    }

    if (process.platform != 'win32') {
        let routes = join(import.meta.dir, 'upload-opt-in-routes.txt')
        try {
            writeFileSync(routes, 'route uri=/ upload=maybe\n')
            let result = spawnSync('goahead', ['--route', routes, 'web', '127.0.0.1:18979'],
                { cwd: join(import.meta.dir, '..'), encoding: 'utf8', timeout: 10000 })
            ttrue(result.status != 0 && result.status != null, 'an invalid upload value must stop goahead')
            if (thas('ME_GOAHEAD_LOGGING')) {
                tcontains(result.stdout + result.stderr, 'upload= must be true or false', 'and say why')
            }
        } finally {
            rmSync(routes, { force: true })
        }
    }
}
