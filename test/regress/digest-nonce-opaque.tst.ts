/*
    digest-nonce-opaque.tst - A Digest nonce reveals nothing about the server secret and cannot be forged
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, thas, ttrue } from '@embedthis/testme'
import { createHash } from 'node:crypto'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port
const TARGET = '/auth/digest/digest.html'
const SAMPLES = 20

function md5(s: string): string {
    return createHash('md5').update(s).digest('hex')
}

async function get(target: string, authorization: string | null = null): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    let auth = authorization ? 'Authorization: ' + authorization + '\r\n' : ''
    await s.write('GET ' + target + ' HTTP/1.1\r\nHost: x\r\n' + auth + 'Connection: close\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function status(raw: string): string {
    let match = raw.match(/^HTTP\/1\.[01] (\d+)/)
    return match ? match[1] : 'none'
}

function field(raw: string, name: string): string {
    let match = raw.match(new RegExp(name + '="([^"]*)"'))
    return match ? match[1] : ''
}

function credential(nonce: string, opaque: string, nc: string): string {
    let cnonce = 'fedcba9876543210'
    let ha1 = md5('joshua:example.com:pass1')
    let response = md5(ha1 + ':' + nonce + ':' + nc + ':' + cnonce + ':auth:' + md5('GET:' + TARGET))
    return `Digest username="joshua", realm="example.com", nonce="${nonce}", uri="${TARGET}", ` +
        `response="${response}", opaque="${opaque}", qop=auth, nc=${nc}, cnonce="${cnonce}", algorithm=MD5`
}

async function challenge(): Promise<{nonce: string, opaque: string, realm: string}> {
    let raw = await get(TARGET)
    ttrue(status(raw) == '401', `an unauthenticated request must be challenged, got ${status(raw)}`)
    return {nonce: field(raw, 'nonce'), opaque: field(raw, 'opaque'), realm: field(raw, 'realm')}
}

if (thas('ME_GOAHEAD_AUTH')) {
    let digests = new Set<string>()
    let lastCounter = -1

    for (let i = 0; i < SAMPLES; i++) {
        let {nonce, realm} = await challenge()
        let decoded = Buffer.from(nonce, 'base64').toString('latin1')
        let match = decoded.match(/^([0-9a-f]{32}):([0-9a-f]{1,8}):([0-9a-f]+)$/)
        ttrue(match != null, `a nonce must decode to digest:time:counter and nothing else, got "${decoded}"`)
        if (!match) {
            continue
        }
        let [, digest, t, c] = match
        ttrue(realm != '' && !decoded.includes(realm), `a nonce must not carry the realm, got "${decoded}"`)

        let when = parseInt(t, 16)
        let now = Math.floor(Date.now() / 1000)
        ttrue(Math.abs(now - when) <= 60, `the nonce time must be current, got ${when} at ${now}`)

        let counter = parseInt(c, 16)
        ttrue(counter > lastCounter, `nonce counters must strictly increase, got ${counter} after ${lastCounter}`)
        lastCounter = counter

        ttrue(!digests.has(digest), `nonce digests must all differ, got ${digest} twice`)
        digests.add(digest)

        //  A digest computable without the secret would let any client forge nonces
        ttrue(digest != md5(':' + realm + ':' + t + ':' + c) && digest != md5(realm + ':' + t + ':' + c),
            `the nonce digest must be bound to the server secret, got ${digest}`)
    }


    let control = await challenge()
    let raw = await get(TARGET, credential(control.nonce, control.opaque, '00000001'))
    ttrue(status(raw) == '200', `an untampered nonce must authenticate, got ${status(raw)}`)

    let fresh = await challenge()
    let decoded = Buffer.from(fresh.nonce, 'base64').toString('latin1')
    let flipped = (decoded[0] == '0' ? '1' : '0') + decoded.slice(1)
    let tampered = Buffer.from(flipped, 'latin1').toString('base64')
    raw = await get(TARGET, credential(tampered, fresh.opaque, '00000001'))
    ttrue(status(raw) == '401', `a nonce with a tampered digest must be refused, got ${status(raw)}`)
}
