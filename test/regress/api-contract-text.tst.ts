/*
    api-contract-text.tst - The goahead.h contract of each security-relevant API describes what the code does

    WEBS_COOKIE_HTTP emits HttpOnly, websSetDebug defeats every timeout, websValidateUriPath returns NULL on a
    refusal, websLookupUpload looks up a part, websAddUser and websSetUserPassword store a hash record, and
    websWriteBlock blocks the server past the response buffer.
 */
import { ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const header = readFileSync(join(ROOT, 'src', 'goahead.h'), 'utf8')

/*
    The doc comment that precedes a PUBLIC declaration
 */
function contract(name: string): string {
    let decl = header.search(new RegExp(`^PUBLIC [^\\n]*\\b${name}\\(`, 'm'))
    ttrue(decl >= 0, `goahead.h declares ${name}`)
    let start = header.lastIndexOf('/**', decl)
    let block = header.slice(start, decl)
    ttrue(start >= 0 && !block.includes('*/\n\n'), `${name} has a doc comment`)
    return block
}

let cookieFlag = header.split('\n').find(l => l.includes('#define WEBS_COOKIE_HTTP')) || ''
ttrue(/HttpOnly/i.test(cookieFlag) && !/http only/i.test(cookieFlag),
    'WEBS_COOKIE_HTTP must be documented as HttpOnly, not as a cookie for plain http')
let setCookie = contract('websSetCookie')
ttrue(/HttpOnly/i.test(setCookie) && !/http only/i.test(setCookie),
    'websSetCookie must document WEBS_COOKIE_HTTP as withholding the cookie from script')

let setDebug = contract('websSetDebug')
ttrue(/timeout/i.test(setDebug), 'websSetDebug must say it disables the request and session timeouts')
ttrue(/production|deploy|never/i.test(setDebug), 'websSetDebug must say it is not for a deployed server')

let validate = contract('websValidateUriPath')
ttrue(/NULL|null/.test(validate) && /(reject|refus)/i.test(validate),
    'websValidateUriPath must say it returns NULL when it rejects the path')

let lookup = contract('websLookupUpload')
ttrue(!/Open the file upload filter/.test(lookup) && /NULL|null/.test(lookup),
    'websLookupUpload must describe the lookup and its NULL result')

ttrue(/BF2/.test(contract('websAddUser')), 'websAddUser must name the stored credential format')
ttrue(/hash/i.test(contract('websSetUserPassword')), 'websSetUserPassword must say it stores a hash record')

ttrue(/ME_GOAHEAD_LIMIT_RESPONSE_BUFFER/.test(contract('websWriteBlock')),
    'websWriteBlock must say it blocks the server past ME_GOAHEAD_LIMIT_RESPONSE_BUFFER')
ttrue(/block/i.test(contract('websWrite')), 'websWrite must say when it blocks')
