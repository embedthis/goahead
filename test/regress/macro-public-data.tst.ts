/*
    macro-public-data.tst.ts - No goahead.h macro reads a PUBLIC_DATA variable

    A macro expands in the unit that uses it. On Windows, a unit that is not built with ME_DLL_IMPORT but links
    libgoahead.dll (the C tests that compile library sources in) declares the variable without dllimport, so a
    macro that reads it leaves an unresolved external. Macros reach library state through an exported function
    instead, as trace() and logmsg() do with websGetLogLevel().
 */
import { tfalse, ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { join } from 'node:path'

const header = readFileSync(join(import.meta.dir, '..', '..', 'src', 'goahead.h'), 'utf8')
const names = [...header.matchAll(/^PUBLIC_DATA\s+[\w\s*]+?\b(\w+)\s*(\[\])?\s*;/gm)].map(m => m[1])
ttrue(names.includes('logLevel'), 'goahead.h declares logLevel as PUBLIC_DATA')

//  Join continuation lines so a multi-line macro is checked whole
const defines = header.replace(/\\\n/g, ' ').split('\n').filter(line => /^\s*#\s*define\s/.test(line))
for (const name of names) {
    const uses = defines.filter(line => new RegExp(`\\b${name}\\b`).test(line))
    tfalse(uses.length > 0, `no macro reads PUBLIC_DATA ${name}: ${uses.map(l => l.trim()).join(' | ')}`)
}
