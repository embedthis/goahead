/*
    credential-file-mode-warning.tst - A password file readable by other accounts is warned about at startup

    The deployment guide asks for the credential file to be mode 0600: a stored MD5 hash is a password
    equivalent for Digest authentication, and an account that can write the file can add an administrator.
    gopass writes the file 0600, but the server loaded a file of any mode in silence. Starts private servers
    on a scratch home and reads their startup output.
 */
import { tcontains, thas, tskip, ttrue } from '@embedthis/testme'
import { chmodSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { startServer, stopServer } from '../utils/server'

//  Outside the tree, so a run that exits early leaves no password file for the shipped-file checks to find
const HOME = mkdtempSync(join(tmpdir(), 'goahead-file-mode-'))
const ENDPOINT = '127.0.0.1:' + (process.env.FILE_MODE_PORT || '18993')
const USERS = 'role name=administrator abilities=manage\nuser name=alice password=x roles=administrator\n'
const WARNING = 'holds passwords and is readable or writable by other accounts'

async function startup(authMode: number, auth = USERS): Promise<{ up: boolean, log: string }> {
    writeFileSync(join(HOME, 'auth.txt'), auth)
    chmodSync(join(HOME, 'auth.txt'), authMode)
    let log = ''
    let server = startServer('goahead', ['--route', 'route.txt', '--auth', 'auth.txt', 'web', ENDPOINT], { cwd: HOME })
    server.stdout?.on('data', (data) => log += data)
    server.stderr?.on('data', (data) => log += data)
    let up = false
    try {
        for (let i = 0; i < 50 && !up && server.exitCode == null; i++) {
            await new Promise(r => setTimeout(r, 100))
            try {
                await fetch(`http://${ENDPOINT}/index.html`, { redirect: 'manual' })
                up = true
            } catch {}
        }
    } finally {
        await stopServer(server)
    }
    return { up, log }
}

if (process.platform == 'win32') {
    tskip('File modes are a POSIX check')
} else if (!thas('ME_GOAHEAD_AUTH') || !thas('ME_GOAHEAD_LOGGING')) {
    tskip('The warning needs a credential file to load and logging to be seen')
} else {
    mkdirSync(join(HOME, 'web'), { recursive: true })
    mkdirSync(join(HOME, 'tmp'), { recursive: true })
    writeFileSync(join(HOME, 'web', 'index.html'), '<html>INDEX</html>\n')
    writeFileSync(join(HOME, 'route.txt'), 'route uri=/\n')
    try {
        let result = await startup(0o644)
        ttrue(result.up, `the server must start: ${result.log}`)
        tcontains(result.log, 'Security warning: "auth.txt" ' + WARNING,
            'a world-readable password file must be warned about, naming the file')
        tcontains(result.log, '0644', 'and the warning must give its mode')

        result = await startup(0o660)
        ttrue(result.up, `the server must start: ${result.log}`)
        tcontains(result.log, WARNING, 'a group-writable password file must be warned about')

        //  Controls: a private password file, and a world-readable file that holds no passwords
        result = await startup(0o600)
        ttrue(result.up, `the server must start: ${result.log}`)
        ttrue(!result.log.includes(WARNING), `a password file of mode 0600 must not be warned about: ${result.log}`)

        result = await startup(0o644, 'role name=administrator abilities=manage\n')
        ttrue(result.up, `the server must start: ${result.log}`)
        ttrue(!result.log.includes(WARNING), `a file without passwords must not be warned about: ${result.log}`)
    } finally {
        rmSync(HOME, { recursive: true, force: true })
    }
}
