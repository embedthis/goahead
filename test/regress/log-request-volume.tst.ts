/* Request-driven failures must not write to the error log at the default level or carry request-sized text. */
import { tget, thas, ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { connect } from 'node:net'
import { join } from 'node:path'

//  setup.sh keeps the test server's log here
const logPath = join(import.meta.dir, '../../server-log.txt')
const url = new URL(tget('TM_HTTP') || 'http://127.0.0.1:18080')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))
const readLog = () => readFileSync(logPath).toString('latin1')
const COUNT = 100

async function request(head: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({host: url.hostname, port: Number(url.port)})
        let data = ''
        socket.setTimeout(3000, () => socket.destroy())
        socket.on('connect', () => socket.write(head))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

//  Run COUNT requests and return the log text they added
async function burst(makeHead: (i: number) => string, status: string): Promise<string> {
    const before = readLog().length
    for (let i = 0; i < COUNT; i++) {
        const response = await request(makeHead(i))
        ttrue(response.includes(status), `request ${i} is refused with ${status}`)
    }
    await sleep(200)
    return readLog().slice(before)
}

const errorLines = (text: string) => text.split('\n').filter(line => line.startsWith('goahead: 0:'))

//  Bad request targets, each with a unique 1,500 byte filler
const filler = (i: number) => `FILL${i}`.padEnd(1500, 'A')
const bad = await burst(i => `GET /a\x01${filler(i)} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`, ' 400 ')
ttrue(errorLines(bad).length === 0, 'bad request targets add no level 0 lines')
ttrue(!bad.includes('FILL'), 'the request target text is never logged')
ttrue(bad.length < COUNT * 200, `bad request targets log a short line each (${bad.length} bytes for ${COUNT})`)
ttrue(bad.length > 0 || !thas('ME_GOAHEAD_LOGGING'), 'bad request targets are still traced at level 2')

if (thas('ME_GOAHEAD_CGI')) {
    const cgi = await burst(i => `GET /cgi-bin/nosuchprogram${i} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`, ' 404 ')
    ttrue(errorLines(cgi).length === 0, 'missing CGI programs add no level 0 lines')
    ttrue(!cgi.includes('Cannot find CGI program'), 'the absolute CGI path is not logged at level 2')
    ttrue(cgi.length < COUNT * 150, `missing CGI programs log a short line each (${cgi.length} bytes for ${COUNT})`)
}
