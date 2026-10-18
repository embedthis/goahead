/*
    redirect-location-encoding.tst - The Location header carries an encoded URI
 */

import { ByteArray, Socket, Uri } from '@embedthis/ejscript'
import { tget, ttrue } from '@embedthis/testme'

const HTTP = tget('TM_HTTP') || '127.0.0.1:18080'
const uri = new Uri(HTTP)
const ADDR = uri.host + ':' + uri.port

async function rawGet(target: string): Promise<string> {
    let s = new Socket
    s.connect(ADDR)
    await s.write('GET ' + target + ' HTTP/1.0\r\nHost: x\r\n\r\n')
    let response = new ByteArray(65536, true)
    try {
        while ((await s.read(response, -1)) != null) {}
    } catch {
    }
    s.close()
    return response.toString()
}

function location(raw: string): string {
    let match = raw.match(/[Ll]ocation: ([^\r\n]*)/)
    return match ? match[1] : ''
}

let raw = await rawGet('/My%20Documents')
ttrue(raw.contains('302'), `a directory must redirect to its index page, got [${raw.slice(0, 40)}]`)
ttrue(location(raw).contains('My%20Documents/index.html'),
      `the Location must be encoded, got [${location(raw)}]`)
ttrue(!location(raw).contains('My Documents'),
      `a raw space must not reach the Location header, got [${location(raw)}]`)

raw = await rawGet('/dir')
ttrue(raw.contains('302'), 'an ordinary directory must still redirect')
ttrue(location(raw).contains('/dir/index.html'), `an ordinary target must be unchanged, got [${location(raw)}]`)

raw = await rawGet('/My%20Documents/index.html')
ttrue(raw.contains('200 OK'), 'the encoded target must resolve')
