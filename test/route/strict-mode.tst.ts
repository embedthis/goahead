/*
    strict-mode.tst - goahead refuses to start with an unsafe route table unless started with --no-strict
 */
import { tcontains, thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { readFileSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

const HOME = join(import.meta.dir, '..')
const SHIPPED = join(import.meta.dir, '..', '..', 'src', 'route.txt')
const ROUTES = join(import.meta.dir, 'strict-mode-routes.txt')
const AUTH = join(import.meta.dir, 'strict-mode-auth.txt')
const ENDPOINT = '127.0.0.1:18976'
const UNSAFE = 'route uri=/secure/ auth=basic\nroute uri=/\n'
const SAFE = 'route uri=/ auth=basic\n'
const FIRST = 'route uri=/\nroute uri=/secure/ auth=basic\n'
const GATE_DIR = 'route uri=/auth/basic/ auth=basic abilities=manage handler=continue dir=private\nroute uri=/\n'

//  The refusals are reported through the logging layer, so a build without logging refuses silently
function logged(output: string, text: string, message: string) {
    if (thas('ME_GOAHEAD_LOGGING')) {
        tcontains(output, text, message)
    }
}

async function serves(args: string[], routes = ROUTES): Promise<{ up: boolean, log: string }> {
    let log = ''
    let server = startServer('goahead', [...args, '--route', routes, 'web', ENDPOINT], { cwd: HOME })
    server.stderr.on('data', (data) => log += data)
    server.stdout.on('data', (data) => log += data)
    let up = false
    try {
        for (let i = 0; i < 50 && !up; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                await fetch(`http://${ENDPOINT}/`)
                up = true
            } catch {}
        }
    } finally {
        await stopServer(server)
    }
    return { up, log }
}

if (!thas('ME_GOAHEAD_AUTH')) {
    tskip('Strict mode checks authenticated routes, and authentication is not enabled')
} else {
    try {
        writeFileSync(ROUTES, UNSAFE)
        for (let args of [[], ['--strict']]) {
            let refused = spawnSync('goahead', [...args, '--route', ROUTES, 'web', ENDPOINT],
                { cwd: HOME, encoding: 'utf8', timeout: 10000 })
            ttrue(refused.status != 0 && refused.status != null,
                `with [${args}] an unsafe table must stop goahead, got status ${refused.status}`)
            let output = refused.stdout + refused.stderr
            logged(output, 'route "/secure/" requires authentication, but later route "/"',
                'the refusal must name the shadowing routes')
            logged(output, 'Strict mode: refusing to start', 'the refusal must say strict mode caused it')
        }

        //  A route that serves content placed before an authenticated route takes its requests first
        writeFileSync(ROUTES, FIRST)
        let first = spawnSync('goahead', ['--route', ROUTES, 'web', ENDPOINT],
            { cwd: HOME, encoding: 'utf8', timeout: 10000 })
        ttrue(first.status != 0 && first.status != null,
            `a catch-all before an authenticated route must stop goahead, got status ${first.status}`)
        logged(first.stdout + first.stderr, 'comes before authenticated route "/secure/"',
            'the refusal must name the earlier route')

        writeFileSync(ROUTES, UNSAFE)
        let result = await serves(['--no-strict'])
        ttrue(result.up, 'with --no-strict an unsafe table still starts')
        logged(result.log, 'Security warning', 'and the unsafe table is warned about')

        //  A continue gate's dir= does not change what the catch-all after it serves
        writeFileSync(ROUTES, GATE_DIR)
        let gate = spawnSync('goahead', ['--route', ROUTES, 'web', ENDPOINT],
            { cwd: HOME, encoding: 'utf8', timeout: 10000 })
        ttrue(gate.status != 0 && gate.status != null,
            `a continue gate with dir= before a catch-all must stop goahead, got status ${gate.status}`)
        logged(gate.stdout + gate.stderr, 'gate route "/auth/basic/" requires authentication',
            'the refusal must name the gate')

        //  The table is checked once complete, so a catch-all loaded from the auth file is checked too
        writeFileSync(ROUTES, 'route uri=/secure/ auth=basic\n')
        writeFileSync(AUTH, readFileSync(join(HOME, 'auth.txt'), 'utf8') + '\nroute uri=/\n')
        let split = spawnSync('goahead', ['--route', ROUTES, '--auth', AUTH, 'web', ENDPOINT],
            { cwd: HOME, encoding: 'utf8', timeout: 10000 })
        ttrue(split.status != 0 && split.status != null,
            `an unsafe table split across the route and auth files must stop goahead, got status ${split.status}`)
        logged(split.stdout + split.stderr, 'route "/secure/" requires authentication, but later route "/"',
            'the refusal must name the route from the auth file')

        writeFileSync(ROUTES, SAFE)
        result = await serves([])
        ttrue(result.up, 'a safe table starts')

        if (thas('ME_GOAHEAD_JAVASCRIPT')) {
            result = await serves([], SHIPPED)
            ttrue(result.up, 'the shipped table, which authenticates nothing, starts')
            logged(result.log, 'no authenticated routes configured', 'and is warned about')
        }
    } finally {
        rmSync(ROUTES, { force: true })
        rmSync(AUTH, { force: true })
    }
}
