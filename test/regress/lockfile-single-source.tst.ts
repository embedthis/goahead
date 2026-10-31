/*
    lockfile-single-source.tst.ts - package-lock.json is the one lockfile; a bun.lock never pins other versions

    CI installs the test dependencies with "npm ci" from package-lock.json, the SBOM records the versions in it, and
    the release archive ships it. The host suite (test/utils/prep-test.sh) and the Linux container
    (test/utils/linux-test.sh) install with bun, which reads a bun.lock when one exists and otherwise migrates
    package-lock.json. A bun.lock that outlives a package-lock.json change therefore installs versions nobody
    reviewed, so both scripts must discard it before a frozen install, and any bun.lock present must agree.
 */
import { tinfo, tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const TOP = join(import.meta.dir, '..', '..')
const NPM_LOCK = join(TOP, 'package-lock.json')
const BUN_LOCK = join(TOP, 'bun.lock')
const SCRIPTS = ['test/utils/prep-test.sh', 'test/utils/linux-test.sh']

if (!existsSync(NPM_LOCK)) {
    tskip('No package-lock.json in this tree')
} else {
    if (!existsSync(BUN_LOCK)) {
        tinfo('No bun.lock beside package-lock.json, so there is no second lockfile to compare')
    } else {
        let npm = JSON.parse(readFileSync(NPM_LOCK, 'utf8')).packages || {}
        let pinned: Record<string, string> = {}
        for (let [key, entry] of Object.entries(npm)) {
            if (key.startsWith('node_modules/')) {
                pinned[key.slice('node_modules/'.length)] = (entry as any).version
            }
        }
        let differ: string[] = []
        for (let match of readFileSync(BUN_LOCK, 'utf8').matchAll(/^\s*"[^"]+":\s*\["((?:@[^/"]+\/)?[^@"]+)@([^"]+)",/gm)) {
            let [, name, version] = match
            if (pinned[name] && pinned[name] != version) {
                differ.push(`${name} ${version} (package-lock.json ${pinned[name]})`)
            }
        }
        ttrue(differ.length == 0, `bun.lock must pin the versions package-lock.json pins: ${differ.join(', ')}`)
    }

    for (let name of SCRIPTS) {
        let path = join(TOP, name)
        if (!existsSync(path)) {
            continue
        }
        let lines = readFileSync(path, 'utf8').split('\n').filter(line => !/^\s*#/.test(line))
        let installs = lines.filter(line => /^\s*bun\s+install\b/.test(line))
        ttrue(installs.length > 0, `${name} must install the test dependencies with bun`)
        for (let line of installs) {
            ttrue(/--frozen-lockfile/.test(line), `${name}: "${line.trim()}" must be a frozen install`)
        }
        let first = lines.findIndex(line => /^\s*bun\s+install\b/.test(line))
        let discards = lines.slice(0, first < 0 ? lines.length : first).some(line => /\brm\s+-f\s+(\S*\/)?bun\.lock\b/.test(line))
        ttrue(discards, `${name} must discard a stale bun.lock before "bun install", so the install derives from package-lock.json`)
    }
}
