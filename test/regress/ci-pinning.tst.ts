/*
    ci-pinning.tst - CI and the Linux test image fetch every action, image and tool by a pinned reference
 */
import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readdirSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const WORKFLOWS = join(ROOT, '.github', 'workflows')
const DOCKERFILE = join(ROOT, 'test', 'utils', 'Dockerfile.linux')

function lines(text: string): string[] {
    return text.split('\n').filter(line => !/^\s*#/.test(line))
}

function noPipedShell(name: string, text: string) {
    let piped = lines(text).filter(line => /\|\s*(ba)?sh\b/.test(line)).map(line => line.trim())
    ttrue(piped.length == 0, `${name}: a download must not be piped to a shell: ${piped}`)
    let global = lines(text).filter(line => /\b(npm|bun|pnpm|yarn)\s+(i|install|add)\b.*\s(-g|--global)\b/.test(line))
    ttrue(global.length == 0, `${name}: a global install is outside the lockfile: ${global.map(line => line.trim())}`)
}

if (!existsSync(WORKFLOWS)) {
    tskip('The CI workflows are not part of a source distribution')
} else {
    let workflows = readdirSync(WORKFLOWS).filter(f => f.endsWith('.yml') || f.endsWith('.yaml'))
    ttrue(workflows.length > 0, 'there must be a workflow to check')

    for (let name of workflows) {
        let text = readFileSync(join(WORKFLOWS, name), 'utf8')
        let uses = lines(text).map(line => line.match(/^\s*(?:-\s+)?uses:\s*(\S+)/)).filter(m => m).map(m => m![1])
        ttrue(uses.length > 0, `${name}: expected to find the actions it uses`)
        let unpinned = uses.filter(ref => !ref.startsWith('./') && !/@[0-9a-f]{40}$/.test(ref))
        ttrue(unpinned.length == 0, `${name}: actions must be pinned by a full commit SHA: ${unpinned}`)
        ttrue(!/^\s*bun-version:/m.test(text), `${name}: bun must come from bun-version-file: .bun-version`)
        ttrue(/^permissions:/m.test(text), `${name}: must set top-level permissions`)
        noPipedShell(name, text)
    }

    let version = existsSync(join(ROOT, '.bun-version')) ? readFileSync(join(ROOT, '.bun-version'), 'utf8').trim() : ''
    ttrue(/^\d+\.\d+\.\d+$/.test(version), `.bun-version must name an exact bun release, got "${version}"`)

    let docker = readFileSync(DOCKERFILE, 'utf8')
    for (let line of lines(docker).filter(line => /^\s*FROM\s/i.test(line))) {
        ttrue(/@sha256:[0-9a-f]{64}\b/.test(line), `Dockerfile.linux: ${line.trim()} must pin the image by digest`)
    }
    ttrue(/sha256sum -c/.test(docker), 'Dockerfile.linux: the bun download must be verified by checksum')
    noPipedShell('Dockerfile.linux', docker)

    let pkg = JSON.parse(readFileSync(join(ROOT, 'package.json'), 'utf8'))
    ttrue((pkg.trustedDependencies || []).includes('@embedthis/testme'),
          'package.json must trust TestMe so a lockfile install builds tm')
}
