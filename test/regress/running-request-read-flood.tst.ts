/*
    Once a request is received, further input must stay in the socket until connection reuse.
    Leave a large response unread and require a concurrent input flood to encounter backpressure.
    The response is a generated file, larger than the socket buffers, so it stays pending. test/web/read-flood.txt
    is git-ignored.
 */

import { tget, ttrue } from '@embedthis/testme'
import { Path, Uri } from '@embedthis/ejscript'
import { connect, Socket } from 'node:net'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const uri = new Uri(HTTP)
const HOST = uri.host
const PORT = parseInt(uri.port)

const CHUNK = Buffer.alloc(65536, 0x41)
const LIMIT = 20000       /* ms before the flood is declared unbounded */
const QUIET = 3000        /* ms without a drain that shows the server has stopped reading */
const CAP = 64 << 20      /* bytes accepted that also show the server has not stopped */
const BODY = '../web/read-flood.txt'
const SIZE = 16 << 20

if (!new Path(BODY).exists || new Path(BODY).size != SIZE) {
    await Bun.write(BODY, Buffer.alloc(SIZE, 0x42))
}

function delay(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms))
}

function open(): Promise<Socket> {
    return new Promise((resolve, reject) => {
        let s = connect(PORT, HOST, () => resolve(s))
        s.once('error', reject)
    })
}

function readResponse(s: Socket): Promise<string> {
    return new Promise(resolve => {
        let raw = ''
        let onData = (data: Buffer) => {
            raw += data.toString('latin1')
            let end = raw.indexOf('\r\n\r\n')
            if (end < 0) {
                return
            }
            let match = raw.slice(0, end).match(/Content-Length: (\d+)/i)
            if (match && raw.length >= end + 4 + parseInt(match[1])) {
                s.removeListener('data', onData)
                resolve(raw)
            }
        }
        s.on('data', onData)
    })
}

async function flood(s: Socket): Promise<{ sent: number, stop: string }> {
    let sent = 0
    let start = Date.now()
    while (Date.now() - start < LIMIT && sent < CAP) {
        if (s.write(CHUNK)) {
            sent += CHUNK.length
            continue
        }
        let drained = await new Promise<boolean>(resolve => {
            let timer = setTimeout(() => { s.removeListener('drain', onDrain); resolve(false) }, QUIET)
            let onDrain = () => { clearTimeout(timer); resolve(true) }
            s.once('drain', onDrain)
        })
        sent += CHUNK.length
        if (!drained) {
            return { sent, stop: 'quiet' }
        }
    }
    return { sent, stop: sent >= CAP ? 'cap' : 'time' }
}

/*
    Second request on a keep-alive connection: the read interest of the first request is in place.
 */
let s = await open()
s.on('error', () => {})
s.write('GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n')
let first = await readResponse(s)
ttrue(first.includes(' 200 '), 'the first request must be served')

s.write('GET /read-flood.txt HTTP/1.1\r\nHost: x\r\n\r\n')
s.pause()
await delay(300)
let result = await flood(s)
s.destroy()
ttrue(result.stop == 'quiet',
      `the server must stop reading while the response is pending, but it accepted ${result.sent} bytes ` +
      `and the flood stopped on "${result.stop}"`)

/*
    First request on a connection whose request line and headers arrive in separate reads.
 */
s = await open()
s.on('error', () => {})
s.write('GET /read-flood.txt HTTP/1.1\r\n')
await delay(200)
s.write('Host: x\r\n\r\n')
s.pause()
await delay(300)
result = await flood(s)
s.destroy()
ttrue(result.stop == 'quiet',
      `the server must stop reading after a request received in two parts, but it accepted ${result.sent} bytes ` +
      `and the flood stopped on "${result.stop}"`)

s = await open()
s.on('error', () => {})
s.write('GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
let after = await readResponse(s)
s.destroy()
ttrue(after.includes(' 200 '), 'the server must still serve afterwards')
