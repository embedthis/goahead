/*
    sec-profile-paths.tst - The security profile's seed-corpus paths exist and hold usable seeds
 */

import { tinfo, ttrue } from '@embedthis/testme'
import { existsSync, readdirSync, readFileSync, statSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const ROOT = fileURLToPath(new URL('../..', import.meta.url))

/* The corpus entries of the profile, with comments removed */
function corpusPaths(profile: string): string[] {
    let text = profile.split('\n').filter(line => !line.trim().startsWith('//')).join('\n')
    let block = text.match(/^\s*corpus:\s*\[([^\]]*)\]/m)
    let paths: string[] = []
    if (block) {
        for (let entry of block[1].matchAll(/'([^']+)'|"([^"]+)"/g)) {
            paths.push(entry[1] || entry[2])
        }
    }
    return paths
}

function seeds(dir: string): string[] {
    let files: string[] = []
    for (let name of readdirSync(dir)) {
        let path = dir + '/' + name
        if (statSync(path).isDirectory()) {
            files.push(...seeds(path))
        } else {
            files.push(path)
        }
    }
    return files
}

/* The profile and the harness directory are not part of the source distribution */
if (!existsSync(ROOT + 'sec-scan.json5')) {
    tinfo('sec-scan.json5 is not present; corpus paths not checked')
} else {
    let paths = corpusPaths(readFileSync(ROOT + 'sec-scan.json5', 'utf8'))
    ttrue(paths.length > 0, 'the profile must name at least one corpus path')
    for (let path of paths) {
        let dir = ROOT + path
        ttrue(existsSync(dir) && statSync(dir).isDirectory(), `the corpus path ${path} must exist`)
        if (!existsSync(dir)) {
            continue
        }
        let files = seeds(dir)
        ttrue(files.length > 0, `the corpus path ${path} must contain at least one seed`)
        for (let file of files) {
            ttrue(statSync(file).size > 0, `the seed ${file.slice(ROOT.length)} must not be empty`)
        }
    }
}
