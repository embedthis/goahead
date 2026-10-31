/*
    status-reason-phrase.tst - every status the server emits carries its own reason phrase, not the 500 text
 */
import { tcontains, tget, thas, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const port = parseInt(new URL(HTTP).port)

async function request(raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({ host: '127.0.0.1', port })
        let data = ''
        socket.setTimeout(5000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

//  A multipart body on a route that does not accept uploads is refused with 415. Without upload support it is not.
if (thas('ME_GOAHEAD_UPLOAD')) {
    const body = '--b\r\nContent-Disposition: form-data; name="f"; filename="f.txt"\r\n\r\nx\r\n--b--\r\n'
    let upload = await request(`POST /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n` +
        `Content-Type: multipart/form-data; boundary=b\r\nContent-Length: ${body.length}\r\n\r\n${body}`)
    let status = upload.split('\r\n')[0]
    tcontains(status, ' 415 ', `an upload to a route without upload=true is refused with 415: ${status}`)
    tcontains(status, 'Unsupported Media Type', `the 415 status line carries its own reason phrase: ${status}`)
    ttrue(!upload.includes('Internal Server Error'), 'the 415 response does not call itself an internal server error')
}

let missing = await request(`GET /no-such-file-here.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`)
tcontains(missing.split('\r\n')[0], '404 Not Found', 'a missing file keeps its reason phrase')
