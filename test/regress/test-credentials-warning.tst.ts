/*
    test-credentials-warning.tst - Shipped files with test accounts say they must never be deployed
 */
import { tcontains, ttrue } from '@embedthis/testme'
import { readdirSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const WARNING = 'NEVER DEPLOY THIS FILE'
const USER = /^\s*user\s+name=/m

function authFiles(dir: string): string[] {
    return (readdirSync(join(ROOT, dir), { recursive: true }) as string[])
        .filter(f => /(^|[\\/])auth[^\\/]*\.txt$/.test(f) && !f.includes('node_modules') && !/^sec[\\/]/.test(f))
        .map(f => join(dir, f))
}

let production = readFileSync(join(ROOT, 'src', 'auth.txt'), 'utf8')
ttrue(!USER.test(production), 'src/auth.txt must have no users')

let files = authFiles('test')
ttrue(files.includes(join('test', 'auth.txt')), 'the test auth.txt is found')
for (let file of files) {
    let text = readFileSync(join(ROOT, file), 'utf8')
    if (USER.test(text)) {
        let header = text.split('\n').slice(0, 10).join('\n')
        tcontains(header, WARNING, `${file} has users, so its header must say it must never be deployed`)
    }
}

let readme = readFileSync(join(ROOT, 'README.md'), 'utf8')
tcontains(readme, 'Never deploy it', 'the README must warn against deploying the test directory')

let gopass = readFileSync(join(ROOT, 'doc', 'man', 'gopass.1'), 'utf8')
ttrue(!gopass.includes('pass1'), 'the gopass example must not use a test account password')
