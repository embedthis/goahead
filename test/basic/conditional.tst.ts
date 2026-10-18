/*
    conditional.tst - Conditional GET and HEAD against the static file handler
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const FILE = '/index.html'
const CONTENT = readFileSync(fileURLToPath(new URL('../web/index.html', import.meta.url)), 'utf8')

interface Reply {
    status: string
    headers: Record<string, string>
    body: string
}

async function request(method: string, headers: string = ''): Promise<Reply> {
    let s = new Socket
    s.connect(ADDR)
    await s.write(method + ' ' + FILE + ' HTTP/1.1\r\nHost: x\r\n' + headers + 'Connection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    let raw = response.toString()
    let end = raw.indexOf('\r\n\r\n')
    let head = end >= 0 ? raw.slice(0, end) : raw
    let reply: Reply = { status: '', headers: {}, body: end >= 0 ? raw.slice(end + 4) : '' }
    let lines = head.split('\r\n')
    reply.status = (lines[0].match(/^HTTP\/1\.[01] (\d+)/) || ['', 'none'])[1]
    for (let line of lines.slice(1)) {
        let colon = line.indexOf(':')
        if (colon > 0) {
            reply.headers[line.slice(0, colon).trim().toLowerCase()] = line.slice(colon + 1).trim()
        }
    }
    return reply
}

let reply = await request('GET')
ttrue(reply.status == '200', `GET must return the file, got ${reply.status}`)
let modified = reply.headers['last-modified']
ttrue(!!modified, 'a file response must carry Last-Modified')
ttrue(/^[A-Z][a-z]{2}, \d{2} [A-Z][a-z]{2} \d{4} \d{2}:\d{2}:\d{2} GMT$/.test(modified || ''),
      `Last-Modified must be an IMF-fixdate, got [${modified}]`)
ttrue(reply.body == CONTENT, 'a 200 must carry the whole file')

reply = await request('GET', 'If-Modified-Since: ' + modified + '\r\n')
ttrue(reply.status == '304', `an unchanged file must return 304, got ${reply.status}`)
ttrue(reply.body == '', `a 304 must have no body, got ${reply.body.length} bytes`)
ttrue(!reply.headers['content-length'] || reply.headers['content-length'] == '0',
      'a 304 must not declare the length of a body it does not send')
ttrue(reply.headers['last-modified'] == modified, 'a 304 still carries Last-Modified')

reply = await request('HEAD', 'If-Modified-Since: ' + modified + '\r\n')
ttrue(reply.status == '304', `HEAD for an unchanged file must return 304, got ${reply.status}`)
ttrue(reply.body == '', 'a HEAD 304 must have no body')

let earlier = new Date(Date.parse(modified!) - 7 * 24 * 3600 * 1000).toUTCString()
reply = await request('GET', 'If-Modified-Since: ' + earlier + '\r\n')
ttrue(reply.status == '200', `a file modified since ${earlier} must return 200, got ${reply.status}`)
ttrue(reply.body == CONTENT, 'the 200 must carry the whole file')

reply = await request('GET')
let etag = reply.headers['etag']
ttrue(/^W\/"[^"]+"$/.test(etag || ''), `a file response must carry a weak ETag, got [${etag}]`)
ttrue((await request('HEAD')).headers['etag'] == etag, 'HEAD must carry the same ETag')
let opaque = (etag || '').slice(2)


for (let [header, why] of [
    ['If-None-Match: ' + etag + '\r\n', 'the weak tag'],
    ['If-None-Match: ' + opaque + '\r\n', 'the strong form of the tag, by weak comparison'],
    ['If-None-Match: *\r\n', '"*"'],
    ['If-None-Match: "x", ' + etag + ' , "y"\r\n', 'the tag within a list'],
    ['If-None-Match: "x"\r\nIf-None-Match: ' + etag + '\r\n', 'the tag in a repeated field'],
]) {
    for (let method of ['GET', 'HEAD']) {
        reply = await request(method, header)
        ttrue(reply.status == '304', `${method} with If-None-Match naming ${why} must return 304, got ${reply.status}`)
        ttrue(reply.body == '', `${method} 304 for ${why} must have no body`)
        ttrue(reply.headers['etag'] == etag, `${method} 304 for ${why} must carry the ETag`)
    }
}


for (let tag of ['"abc"', 'W/"abc"', '"x", "y"', opaque + 'x']) {
    reply = await request('GET', 'If-None-Match: ' + tag + '\r\n')
    ttrue(reply.status == '200', `If-None-Match ${tag} must return 200, got ${reply.status}`)
    ttrue(reply.body == CONTENT, 'the 200 must carry the whole file')
    ttrue(reply.headers['etag'] == etag, 'the 200 must carry the ETag')
}
reply = await request('GET', 'If-None-Match: "abc"\r\nIf-Modified-Since: ' + modified + '\r\n')
ttrue(reply.status == '200', `a non-matching If-None-Match must override If-Modified-Since, got ${reply.status}`)
ttrue(reply.body == CONTENT, 'the 200 must carry the whole file')
