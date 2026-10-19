/* Exercise the shipped routing configuration, not the test server route file. */
import {ttrue, thas, tskip} from '@embedthis/testme'
import {createServer} from 'node:net'
import {get} from 'node:http'
import {join} from 'node:path'
import {startServer, stopServer} from '../utils/server'

async function request(port: number, path: string): Promise<{status:number, body:string}> {
    return await new Promise((resolve, reject) => {
        const req = get({host:'127.0.0.1', port, path}, res => {
            let body = ''
            res.on('data', data => body += data)
            res.on('end', () => resolve({status:res.statusCode || 0, body}))
        })
        req.on('error', reject)
        req.setTimeout(3000, () => req.destroy(new Error('request timed out')))
    })
}
if (!thas('ME_GOAHEAD_JAVASCRIPT')) {
    tskip('JST disabled')
} else {
    const probe = createServer()
    await new Promise<void>(resolve => probe.listen(0, '127.0.0.1', resolve))
    const port = (probe.address() as any).port
    await new Promise<void>(resolve => probe.close(() => resolve()))
    const root = join(import.meta.dir, '..', '..')
    const child = startServer(join(root, 'build/bin/goahead-test'),
        ['--route', join(root, 'src/route.txt'), 'web', `http://127.0.0.1:${port}`],
        {cwd:join(root, 'test'), stdio:'ignore'})
    try {
        let ready = false
        for (let i=0; i<100; i++) {
            try { await request(port, '/index.html'); ready=true; break } catch {}
            await new Promise(resolve => setTimeout(resolve, 25))
        }
        ttrue(ready)
        for (const path of ['/root-template.jst', '/root-template.asp', '/jst/text.jst']) {
            const response = await request(port, path)
            ttrue(response.status == 200)
            ttrue(!response.body.includes('<%'))
            ttrue(response.body.includes(path == '/jst/text.jst' ? 'ABC' : 'rendered-template'))
        }
    } finally {
        await stopServer(child)
    }
}
