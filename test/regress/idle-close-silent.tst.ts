/* An idle connection with no request begun is closed without a response */

import { Uri, Config } from '@embedthis/ejscript'
import { teq, tget, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'

const HTTP = tget('TM_HTTP') || 'http://127.0.0.1:18080'
const uri = new Uri(HTTP)
const HOST = uri.host
const PORT = uri.port

const PAST_IDLE = 75000

function sleep(ms: number): Promise<void> {
    return new Promise(r => setTimeout(r, ms))
}

function open(): any {
    let h: any = {data: '', closed: false}
    h.sock = connect(PORT, HOST)
    h.sock.on('data', (d: any) => h.data += d)
    h.sock.on('error', () => { h.closed = true })
    h.sock.on('close', () => { h.closed = true })
    h.send = (s: string) => { try { h.sock.write(s) } catch {} }
    h.done = () => { try { h.sock.destroy() } catch {} }
    return h
}

async function waitFor(test: () => boolean, ms: number): Promise<void> {
    for (let end = Date.now() + ms; !test() && Date.now() < end; ) {
        await sleep(100)
    }
}

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('waits past the 60 second idle timeout; too slow for the Windows runner')
} else {
    let silent = open()
    let kept = open()
    let partial = open()
    try {
        kept.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n')
        await waitFor(() => /\r\n\r\n[\s\S]*<\/html>/i.test(kept.data), 5000)
        ttrue(kept.data.startsWith('HTTP/1.1 200'), 'the keep-alive request must be served')
        let served = kept.data.length

        partial.send('GET /index.html HTTP/1.1\r\nHost: localhost\r\n')

        await waitFor(() => silent.closed && kept.closed && partial.closed, PAST_IDLE)

        ttrue(silent.closed, 'a connection that sent nothing must be closed at the idle timeout')
        teq(silent.data, '', 'and must receive no response')

        ttrue(kept.closed, 'an idle keep-alive connection must be closed at the idle timeout')
        teq(kept.data.length, served, 'and must receive nothing after its last response')

        ttrue(partial.closed, 'a request that has begun must be closed when it times out')
        ttrue(/^HTTP\/1\.[01] 408/.test(partial.data),
              `and must be answered with 408, got "${partial.data.slice(0, 40)}"`)
    } finally {
        silent.done()
        kept.done()
        partial.done()
    }
}
