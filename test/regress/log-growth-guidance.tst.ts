/*
    log-growth-guidance.tst - The deployment guide says what an unauthenticated client can write to the level-0 log,
    and how the access log is enabled and where it is written

    A client needs no account to send an Authorization header, and each refusal is one level-0 line, so the guide
    must not say such requests cannot grow a level-0 log. Starts a private server to measure it, then checks the text.
 */
import { tcontains, tget, thas, tinfo, ttrue } from '@embedthis/testme'
import { mkdtempSync, readFileSync, rmSync, statSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const ROOT = join(import.meta.dir, '..', '..')
const TEST = join(ROOT, 'test')
const PORT = process.env.LOG_GROWTH_PORT || '18973'
const ROUTES = String(tget('TM_ROUTES'))
const ATTEMPTS = 20

async function up(): Promise<boolean> {
    for (let i = 0; i < 50; i++) {
        try {
            await fetch(`http://127.0.0.1:${PORT}/index.html`)
            return true
        } catch {
            await new Promise(r => setTimeout(r, 100))
        }
    }
    return false
}

if (!thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_LOGGING')) {
    tinfo('Authentication or logging is not compiled in: the log growth is not measured')
} else {
    const LOGDIR = mkdtempSync(join(tmpdir(), 'goahead-log-growth-'))
    const LOG = join(LOGDIR, 'level0.log')
    let server = startServer('goahead', ['--no-strict', '--route', ROUTES, '--auth', 'auth.txt', '--log', `${LOG}:0`,
        'web', `127.0.0.1:${PORT}`], { cwd: TEST })
    try {
        ttrue(await up(), 'the private server starts')
        let before = statSync(LOG).size
        for (let i = 0; i < ATTEMPTS; i++) {
            //  A Basic credential for an account that does not exist: no session, no authentication
            let res = await fetch(`http://127.0.0.1:${PORT}/auth/basic/index.html`,
                { headers: { Authorization: 'Basic ' + Buffer.from('nobody:synthetic').toString('base64') } })
            ttrue(res.status == 401, `an unknown account is refused, got ${res.status}`)
        }
        await new Promise(r => setTimeout(r, 200))
        let growth = statSync(LOG).size - before
        ttrue(growth >= ATTEMPTS * 50, `${ATTEMPTS} unauthenticated requests grew the level-0 log by ${growth} bytes`)
    } finally {
        await stopServer(server)
        rmSync(LOGDIR, { recursive: true, force: true })
    }
}

let guide = readFileSync(join(ROOT, 'SECURITY-DEPLOYMENT.md'), 'utf8')
let logging = guide.slice(guide.indexOf('## Logging'), guide.indexOf('## Routes and authentication'))
ttrue(logging.length > 0, 'the guide has a Logging section before Routes and authentication')
ttrue(!/cannot grow the log/.test(logging),
    'the guide must not say that requests a client can send without authenticating cannot grow the level-0 log')
ttrue(/(no account|unauthenticated client)[^\n]*grow[^\n]*level.0 log/.test(logging),
    'the guide must say an unauthenticated client can grow the level-0 log through authentication refusals')
tcontains(logging, 'ME_GOAHEAD_ACCESS_LOG', 'the guide must say how the access log is enabled')
ttrue(/access\.log[^\n]*working directory/.test(logging), 'the guide must say where the access log is written')
