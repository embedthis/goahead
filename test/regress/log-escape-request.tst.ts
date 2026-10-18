/* A request target with control bytes must write one short escaped line to the error log. */
import { tget, thas, ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { connect } from 'node:net'
import { join } from 'node:path'

//  setup.sh keeps the test server's log here
const logPath = join(import.meta.dir, '../../server-log.txt')
const url = new URL(tget('TM_HTTP') || 'http://127.0.0.1:18080')
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))
const readLog = () => readFileSync(logPath).toString('latin1')

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

const before = readLog().length
const response = await request('GET /a\ngoahead:\t0:\tFORGED-LOGIN-OK\r\x1b[2J HTTP/1.1\r\nHost: localhost\r\n\r\n')
ttrue(response.includes(' 400 '), 'a request target with control bytes is rejected')
await sleep(100)

const added = readLog().slice(before)
if (thas('ME_GOAHEAD_LOGGING')) {
    ttrue(added.split('\n').filter(line => line.includes('Bad character')).length === 1,
        'the rejected request target is logged on exactly one line')
    ttrue(added.includes('Bad character 0x0a in URI at offset 2'), 'the log names the offending byte and offset')
}
ttrue(!/[\x00-\x09\x0b-\x1f\x7f]/.test(added), 'no raw control byte reaches the log')
ttrue(!added.includes('FORGED-LOGIN-OK'), 'the request target is not logged')
