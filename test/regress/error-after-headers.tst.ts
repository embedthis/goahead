/*
    An error after committed headers must close the connection without inserting another response.
 */
import { tcontains, thas, tskip, ttrue } from '@embedthis/testme'
import { connect } from 'node:net'
import { mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const DIR = join(import.meta.dir, 'error-after-headers.tmp')
const PORT = 18984

async function request(raw: string): Promise<string> {
    return new Promise(resolve => {
        const socket = connect({ host: '127.0.0.1', port: PORT })
        let data = ''
        socket.setTimeout(5000, () => socket.destroy())
        socket.on('connect', () => socket.write(raw))
        socket.on('data', chunk => data += chunk.toString())
        socket.on('error', () => {})
        socket.on('close', () => resolve(data))
    })
}

if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('The in-tree trigger is a template built-in, and templates are not enabled')
} else {
    mkdirSync(join(DIR, 'web'), { recursive: true })
    //  escape() takes one argument; two make it report an error after the literal text has started the response
    writeFileSync(join(DIR, 'web', 'broken.jst'), 'start-of-body<% escape("a", "b"); %>tail\n')
    writeFileSync(join(DIR, 'web', 'index.html'), 'index\n')
    writeFileSync(join(DIR, 'route.txt'), 'route uri=/ extensions=jst handler=jst\nroute uri=/\n')
    const auth = thas('ME_GOAHEAD_AUTH') ? ['--auth', join(import.meta.dir, '..', 'auth.txt')] : []
    const server = startServer('goahead', ['--no-strict', '--route', join(DIR, 'route.txt'), ...auth, 'web',
        `http://127.0.0.1:${PORT}`], { cwd: DIR })
    try {
        let up = ''
        for (let i = 0; i < 50 && !up.includes(' 200 '); i++) {
            await new Promise(r => setTimeout(r, 100))
            up = await request('GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        }
        ttrue(up.includes(' 200 '), 'the private server starts')

        let response = await request('GET /broken.jst HTTP/1.1\r\nHost: localhost\r\n\r\n')
        let status = response.split('\r\n')[0]
        if (status.includes(' 200 ')) {
            //  The headers had reached the wire: the response is cut short, never completed with an error page
            ttrue(!response.includes('Access Error') && !response.includes('Internal Server Error'),
                'no error page is written into the committed response')
            ttrue(!response.includes('\r\n0\r\n\r\n'), 'the abandoned response is not framed as complete')
        } else {
            //  The headers were still buffered: the error replaces them as a whole response
            tcontains(status, ' 500 ', `a response not yet on the wire is replaced by the error: ${status}`)
            ttrue(!response.includes('start-of-body'), 'the discarded body does not precede the error page')
        }

        response = await request('GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        tcontains(response, 'index', 'the server still serves the next connection')
    } finally {
        await stopServer(server)
        rmSync(DIR, { recursive: true, force: true })
    }
}
