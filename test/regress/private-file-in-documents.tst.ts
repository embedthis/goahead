/*
    Refuse startup when a TLS private key or log file lies inside a served directory.
 */
import { thas, tskip, ttrue } from '@embedthis/testme'
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, 'private-file-in-documents.dir')
const PKI = join(import.meta.dir, '..', 'pki')
const ENDPOINT = '127.0.0.1:' + (process.env.PRIVATE_FILE_PORT || '18995')
const AUTH = thas('ME_GOAHEAD_AUTH') ? ['--auth', 'auth.txt'] : []

type Result = { up: boolean, log: string, fetch: (path: string) => Promise<{ status: number, body: string }> }

async function startup(args: string[], logFile?: string): Promise<Result> {
    let log = ''
    let server = startServer('goahead', ['--no-strict', '--route', 'route.txt', ...AUTH, ...args, 'web', ENDPOINT],
        { cwd: HOME })
    server.stdout?.on('data', (data) => log += data)
    server.stderr?.on('data', (data) => log += data)
    let up = false
    for (let i = 0; i < 50 && !up && server.exitCode == null; i++) {
        await new Promise(r => setTimeout(r, 100))
        try {
            await fetch(`http://${ENDPOINT}/index.html`, { redirect: 'manual' })
            up = true
        } catch {}
    }
    return {
        up,
        get log() {
            //  With --log to a file the refusal is written there, not to stdout
            return log + (logFile && existsSync(logFile) ? readFileSync(logFile, 'utf8') : '')
        },
        fetch: async (path: string) => {
            let status = 0, body = ''
            try {
                if (up) {
                    let response = await fetch(`http://${ENDPOINT}${path}`, { redirect: 'manual' })
                    body = await response.text()
                    status = response.status
                }
            } finally {
                await stopServer(server)
            }
            return { status, body }
        },
    }
}

async function check(name: string, args: string[], path: string, marker: string, logFile?: string) {
    let result = await startup(args, logFile)
    let { status, body } = await result.fetch(path)
    ttrue(!result.up || status != 200,
        `${name}: the server must refuse to start or must not serve ${path}, got ${status}: ${result.log}`)
    ttrue(!body.includes(marker), `${name}: the content of ${path} must not be sent`)
    if (!result.up && thas('ME_GOAHEAD_LOGGING')) {
        ttrue(result.log.includes(path.slice(1)) && /inside the served directory/.test(result.log),
            `${name}: the refusal must name the file and say it is inside a served directory: ${result.log}`)
    }
}

if (process.platform == 'win32') {
    tskip('The goahead program is driven with POSIX paths here')
} else if (!existsSync(join(PKI, 'test.key'))) {
    tskip('The test PKI is not generated (run test/prep.sh)')
} else {
    rmSync(HOME, { recursive: true, force: true })
    mkdirSync(join(HOME, 'web'), { recursive: true })
    mkdirSync(join(HOME, 'private'), { recursive: true })
    writeFileSync(join(HOME, 'web', 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(HOME, 'route.txt'), 'route uri=/\n')
    writeFileSync(join(HOME, 'auth.txt'), 'role name=administrator abilities=manage\n')
    copyFileSync(join(PKI, 'test.key'), join(HOME, 'web', 'zkey.pem'))
    copyFileSync(join(PKI, 'test.crt'), join(HOME, 'web', 'zcert.pem'))
    copyFileSync(join(PKI, 'test.key'), join(HOME, 'private', 'zkey.pem'))
    copyFileSync(join(PKI, 'test.crt'), join(HOME, 'private', 'zcert.pem'))
    try {
        if (thas('ME_SSL')) {
            await check('TLS key in the documents', ['--key', 'web/zkey.pem', '--cert', 'web/zcert.pem'], '/zkey.pem',
                'PRIVATE KEY')
        }
        if (thas('ME_GOAHEAD_LOGGING')) {
            await check('log file in the documents', ['--log', 'web/zlog.txt:2'], '/zlog.txt', 'goahead:',
                join(HOME, 'web', 'zlog.txt'))
        }

        //  Control: the same files outside the served directory load, and the documents are served
        for (let name of ['zkey.pem', 'zcert.pem', 'zlog.txt']) {
            rmSync(join(HOME, 'web', name), { force: true })
        }
        let tls = thas('ME_SSL') ? ['--key', 'private/zkey.pem', '--cert', 'private/zcert.pem'] : []
        let result = await startup([...tls, '--log', 'private/zlog.txt:2'])
        ttrue(result.up, `the key and log outside the documents must load: ${result.log}`)
        let { status } = await result.fetch('/zkey.pem')
        ttrue(status == 404, `nothing named zkey.pem is in the documents, got ${status}`)
    } finally {
        rmSync(HOME, { recursive: true, force: true })
    }
}
