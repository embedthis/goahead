/*
    half-close-response.tst - A client that half-closes after its request still receives the whole response

    Invariant: an EOF on the connection after the request has been received in full does not cancel the write
    interest that completes the response. On a keep-alive connection the read interest of the previous request
    survived into the running state, so the EOF was read while the server waited to write, and the read path
    dropped every socket interest. The response then stalled until the idle timeout closed the connection.

    Oracle: with the server blocked on write (the client stops reading for a moment), the client shuts down its
    sending side and resumes reading. The response must keep flowing: at least eight megabytes within ten seconds.
    The response is a generated file, larger than the socket buffers plus WANT. test/web/half-close.txt is git-ignored.
 */

import { tget, ttrue } from '@embedthis/testme'
import { Path, Uri } from '@embedthis/ejscript'
import { connect, Socket } from 'node:net'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const uri = new Uri(HTTP)
const HOST = uri.host
const PORT = parseInt(uri.port)

const WANT = 8 << 20      /* bytes that must arrive after the half-close */
const LIMIT = 10000       /* ms allowed for them */
const BODY = '../web/half-close.txt'
const SIZE = 32 << 20

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

function firstData(s: Socket): Promise<Buffer> {
    return new Promise(resolve => s.once('data', resolve))
}

/*
    Count the bytes that arrive after the half-close, for up to LIMIT ms or until WANT bytes.
 */
function drain(s: Socket): Promise<number> {
    return new Promise(resolve => {
        let got = 0
        let timer = setTimeout(() => resolve(got), LIMIT)
        s.on('data', (data: Buffer) => {
            got += data.length
            if (got >= WANT) {
                clearTimeout(timer)
                resolve(got)
            }
        })
        s.on('close', () => { clearTimeout(timer); resolve(got) })
    })
}

let s = await open()
s.on('error', () => {})
s.write('GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n')
let first = await readResponse(s)
ttrue(first.includes(' 200 '), 'the first request must be served')

s.write('GET /half-close.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n')
let head = await firstData(s)
ttrue(head.toString('latin1').includes(' 200 '), 'the large document must be served')

/*
    Let the server fill the socket buffers and block on write, then half-close and resume reading.
 */
s.pause()
await delay(1500)
s.end()
await delay(500)
s.resume()
let got = await drain(s)
s.destroy()
ttrue(got >= WANT, `the response must continue after the client half-closes, got ${got} bytes in ${LIMIT} ms`)
