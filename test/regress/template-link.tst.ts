/*
    template-link.tst - A link that gives a template another extension does not serve its source

    The file handler never serves template source, and it judges that by the request's extension. A link
    inside the documents whose name has another extension resolves to the stored template, so the file handler
    must judge the stored name too, as it already does for hidden and backup names. Starts a private server
    because the shared test server's documents are not to be linked into.
 */
import { Config } from '@embedthis/ejscript'
import { thas, tskip, ttrue } from '@embedthis/testme'
import { mkdirSync, rmSync, symlinkSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'template-link.dir')
const WEB = join(HOME, 'web')
const ENDPOINT = '127.0.0.1:' + (process.env.TEMPLATE_LINK_PORT || '18994')

if (Config.OS == 'windows' || Config.OS == 'win32') {
    tskip('links need privilege on Windows, and stored names are judged on POSIX systems only')
} else {
    const JST_ROUTE = thas('ME_GOAHEAD_JAVASCRIPT') ? 'route uri=/ extensions=jst,asp handler=jst\n' : ''

    rmSync(HOME, { recursive: true, force: true })
    mkdirSync(WEB, { recursive: true })
    writeFileSync(join(WEB, 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(WEB, 'page.jst'), '<% write("RENDERED"); %>')
    symlinkSync('page.jst', join(WEB, 'alias.txt'))
    symlinkSync('page.jst', join(WEB, 'alias'))
    symlinkSync('index.html', join(WEB, 'inlink.html'))
    writeFileSync(join(HOME, 'route.txt'), JST_ROUTE + 'route uri=/\n')
    writeFileSync(join(HOME, 'auth.txt'), '')

    const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []
    let log = ''
    let server = startServer('goahead', ['-v', '--no-strict', '--route', 'route.txt', ...AUTH, 'web', ENDPOINT],
        { cwd: HOME })
    server.stdout?.on('data', (data) => log += data)
    server.stderr?.on('data', (data) => log += data)

    try {
        let index: Response | null = null
        for (let i = 0; i < 50 && !index; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                index = await fetch(`http://${ENDPOINT}/index.html`)
            } catch {}
        }
        ttrue(index != null && index.status == 200, `the server must start: ${log}`)

        for (let name of ['alias.txt', 'alias']) {
            let response = await fetch(`http://${ENDPOINT}/${name}`)
            let text = await response.text()
            ttrue(response.status == 404, `a link to a template under another name must be refused, got ${response.status}`)
            ttrue(!text.includes('<%'), `GET /${name} must not return the template source`)
        }

        //  A link to an ordinary document inside the root is still served
        let link = await fetch(`http://${ENDPOINT}/inlink.html`)
        ttrue(link.status == 200 && (await link.text()).includes('INDEX'),
              `a link to a document inside the root must be served, got ${link.status}`)

        if (thas('ME_GOAHEAD_JAVASCRIPT')) {
            let page = await fetch(`http://${ENDPOINT}/page.jst`)
            let text = await page.text()
            ttrue(page.status == 200 && text.includes('RENDERED') && !text.includes('<%'),
                  `the template itself must still run, got ${page.status}`)
        }
    } finally {
        await stopServer(server)
        rmSync(HOME, { recursive: true, force: true })
    }
}
