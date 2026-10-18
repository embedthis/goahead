/*
    header-platform-order.tst.ts - goahead.h tests no osdep.h macro before including osdep.h

    A platform macro such as ME_WIN_LIKE is undefined, so 0, before osdep.h defines it. A default chosen by it
    there silently takes the other branch on that platform.
 */

import { tskip, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const SRC = join(import.meta.dir, '..', '..', 'src')
const HEADER = join(SRC, 'goahead.h')
const OSDEP = join(SRC, 'osdep', 'osdep.h')

if (!existsSync(HEADER) || !existsSync(OSDEP)) {
    tskip('src/goahead.h and src/osdep/osdep.h are not present')
} else {
    const header = readFileSync(HEADER, 'utf8')
    const include = header.search(/^#include\s+"osdep\.h"/m)
    ttrue(include > 0, 'goahead.h must include osdep.h')

    const defined = new Set<string>()
    for (const match of readFileSync(OSDEP, 'utf8').matchAll(/^\s*#\s*define\s+([A-Za-z_]\w*)/gm)) {
        defined.add(match[1])
    }
    const early: string[] = []
    for (const line of header.slice(0, include).split('\n')) {
        const cond = line.match(/^\s*#\s*(?:if|elif)\s+(.*)$/)
        if (!cond) {
            continue
        }
        for (const name of cond[1].match(/[A-Za-z_]\w*/g) || []) {
            if (name != 'defined' && defined.has(name)) {
                early.push(`${name} in "${line.trim()}"`)
            }
        }
    }
    ttrue(early.length == 0, `goahead.h tests osdep.h macros before including it: ${early.join('; ')}`)
}
