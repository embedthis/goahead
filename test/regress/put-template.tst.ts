/*
    PUT must refuse template creation, even when its route precedes the template handler.
 */
import { thas, ttrue } from '@embedthis/testme'
import { existsSync, mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'put-template.dir')
const WEB = join(HOME, 'web')
const ENDPOINT = '127.0.0.1:' + (process.env.PUT_TEMPLATE_PORT || '18986')
const TEMPLATE = '<% write("TEMPLATE-RAN"); %>'

//  A build without the template handler cannot carry a jst route; the PUT refusal does not depend on it
const JST_ROUTE = thas('ME_GOAHEAD_JAVASCRIPT') ? 'route uri=/ extensions=jst,asp handler=jst\n' : ''

rmSync(HOME, { recursive: true, force: true })
mkdirSync(join(WEB, 'tmp'), { recursive: true })
writeFileSync(join(WEB, 'index.html'), '<html>INDEX</html>\n')
writeFileSync(join(HOME, 'route.txt'), 'route uri=/tmp/ methods=PUT|DELETE\n' + JST_ROUTE + 'route uri=/\n')
writeFileSync(join(HOME, 'auth.txt'), '')

const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []
let log = ''
let server = startServer('goahead', ['-v', '--no-strict', '--route', 'route.txt', ...AUTH, 'web', ENDPOINT], { cwd: HOME })
server.stdout?.on('data', (data) => log += data)
server.stderr?.on('data', (data) => log += data)

async function put(path: string, body: string): Promise<number> {
    let response = await fetch(`http://${ENDPOINT}${path}`, { method: 'PUT', body })
    await response.text()
    return response.status
}

try {
    let index: Response | null = null
    for (let i = 0; i < 50 && !index; i++) {
        await new Promise(r => setTimeout(r, 100))
        try {
            index = await fetch(`http://${ENDPOINT}/index.html`)
        } catch {}
    }
    ttrue(index != null && index.status == 200, `the server must start: ${log}`)

    for (let name of ['zput-template.jst', 'zput-template.JST', 'zput-template.asp']) {
        let status = await put(`/tmp/${name}`, TEMPLATE)
        ttrue(status == 404, `PUT of the template ${name} must be refused, got ${status}`)
        ttrue(!existsSync(join(WEB, 'tmp', name)), `the refused PUT must not create ${name}`)

        let response = await fetch(`http://${ENDPOINT}/tmp/${name}`)
        let text = await response.text()
        ttrue(!text.includes('TEMPLATE-RAN') && !text.includes('<%'),
              `GET /tmp/${name} must neither run nor return the stored template, got ${response.status}`)
    }

    let status = await put('/tmp/zput-template.txt', 'plain')
    ttrue(status == 201 || status == 204, `PUT of a plain document must still succeed, got ${status}`)
    ttrue(existsSync(join(WEB, 'tmp', 'zput-template.txt')), 'the plain PUT must create the document')
} finally {
    await stopServer(server)
    rmSync(HOME, { recursive: true, force: true })
}
