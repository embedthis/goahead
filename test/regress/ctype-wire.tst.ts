/* Exercise header rejection through the server, with literal high-bit bytes. */
import { tget, teq } from '@embedthis/testme'
import { connect } from 'node:net'

const endpoint = new URL(tget('TM_HTTP'))
async function status(header: Buffer): Promise<number> {
    return new Promise(resolve => {
        const socket = connect({host: endpoint.hostname, port: Number(endpoint.port)})
        let response = ''
        socket.setTimeout(3000, () => socket.destroy())
        socket.on('connect', () => socket.write(Buffer.concat([
            Buffer.from('GET /index.html HTTP/1.0\r\nHost: localhost\r\n'), header, Buffer.from('\r\n\r\n')
        ])))
        socket.on('data', data => response += data.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(Number(response.match(/^HTTP\/1\.[01] (\d+)/)?.[1] || 0)))
    })
}
teq(await status(Buffer.from([0x58, 0xE2, 0x3A, 0x20, 0x61])), 400, 'high-bit header name rejected')
teq(await status(Buffer.from([0x58, 0x3A, 0x20, 0xE2])), 400, 'high-bit header value rejected')
teq(await status(Buffer.from('X-Test: ascii')), 200, 'ordinary header remains accepted')
