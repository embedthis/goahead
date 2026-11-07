/*
    access-log-in-documents.tst - An ME_GOAHEAD_ACCESS_LOG build refuses to serve its own access log

    The access log is written as access.log in the working directory, so a server started with the documents
    directory as its working directory writes every client's request line into a served file. The served-directory
    audit holds the route table, the credential file, the TLS key and the error log to this rule; the access log is
    the same class.
 */
import { tskip, ttrue } from '@embedthis/testme'
import { ChildProcess } from 'node:child_process'
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { get } from 'node:http'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { cc, compile, isWindows, ROOT } from '../utils/amalgamation'
import { startServer, stopServer } from '../utils/server'

const PORT = Number(process.env.ACCESS_LOG_PORT || 18997)
const MARKER = 'access-log-marker'

async function request(path: string): Promise<{ status: number, body: string }> {
    return await new Promise((resolve, reject) => {
        const req = get({ host: '127.0.0.1', port: PORT, path }, res => {
            let body = ''
            res.on('data', data => body += data)
            res.on('end', () => resolve({ status: res.statusCode || 0, body }))
        })
        req.on('error', reject)
        req.setTimeout(3000, () => req.destroy(new Error('request timed out')))
    })
}

/*
    Start the server in cwd and wait for it to answer or exit. Returns whether it came up and what it logged.
 */
async function startup(exe: string, cwd: string, args: string[]): Promise<{ server: ChildProcess, up: boolean, log: () => string }> {
    let log = ''
    let server = startServer(exe, [...args, `127.0.0.1:${PORT}`], { cwd, stdio: ['ignore', 'pipe', 'pipe'] })
    server.stdout?.on('data', data => log += data)
    server.stderr?.on('data', data => log += data)
    let up = false
    for (let i = 0; i < 100 && !up && server.exitCode == null; i++) {
        try {
            up = (await request('/index.html')).status == 200
        } catch {
            await new Promise(resolve => setTimeout(resolve, 25))
        }
    }
    return { server, up, log: () => log }
}

if (isWindows) {
    tskip('Rebuilding the amalgamation is a POSIX-only test')
} else if (!cc) {
    tskip('No C compiler on PATH to rebuild the amalgamation at ME_GOAHEAD_ACCESS_LOG=1')
} else {
    const dir = mkdtempSync(join(tmpdir(), 'goahead-accesslog-'))
    const web = join(dir, 'web')
    const put = join(dir, 'put')
    mkdirSync(web)
    mkdirSync(put)
    writeFileSync(join(web, 'index.html'), 'access-log-index\n')
    writeFileSync(join(dir, 'route.txt'), 'route uri=/\n')
    writeFileSync(join(dir, 'auth.txt'), '')

    /*
        Stage PUT bodies and uploads outside the working directory, so the only server-owned file left inside
        it is the access log, which has no setting of its own.
     */
    const exe = join(dir, 'goahead-accesslog')
    const build = compile(exe, `-DME_GOAHEAD_ACCESS_LOG=1 -DME_GOAHEAD_PUT_DIR=\\"${put}\\" -DME_GOAHEAD_UPLOAD_DIR=\\"\\"`,
        join(ROOT, 'src', 'goahead.c'))
    ttrue(build.status == 0, 'Cannot build the ME_GOAHEAD_ACCESS_LOG=1 server:\n' + String(build.error))
    if (build.status == 0) {
        try {
            //  The working directory is the documents directory, so access.log is created inside it
            let { server, up, log } = await startup(exe, web, ['--no-strict', '--route', '../route.txt', '--auth', '../auth.txt', '.'])
            try {
                if (up) {
                    await request(`/index.html?${MARKER}`)
                    let { status, body } = await request('/access.log')
                    ttrue(status != 200, `the access log inside the documents must not be served, got ${status}`)
                    ttrue(!body.includes(MARKER), 'the access log must not reach a client')
                } else {
                    ttrue(/access\.log/.test(log()) && /inside the served directory/.test(log()),
                        'the refusal must name access.log and say it is inside a served directory: ' + log())
                }
            } finally {
                await stopServer(server)
            }
            //  websOpen creates the access log before the audit runs, so a refused start leaves it behind
            rmSync(join(web, 'access.log'), { force: true })

            //  Control: with the documents in a subdirectory the access log stays private and the server serves
            ;({ server, up, log } = await startup(exe, dir, ['--no-strict', '--route', 'route.txt', '--auth', 'auth.txt', 'web']))
            try {
                ttrue(up, 'the server must start when the access log is outside the documents: ' + log())
                if (up) {
                    let { status } = await request('/access.log')
                    ttrue(status == 404, `nothing named access.log is in the documents, got ${status}`)
                }
            } finally {
                await stopServer(server)
            }
        } finally {
            rmSync(dir, { recursive: true, force: true })
        }
    }
}
