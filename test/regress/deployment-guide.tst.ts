/*
    deployment-guide.tst - The deployment guide ships, and everything it names exists
 */
import { tcontains, ttrue } from '@embedthis/testme'
import { existsSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const GUIDE = join(ROOT, 'SECURITY-DEPLOYMENT.md')

ttrue(existsSync(GUIDE), 'SECURITY-DEPLOYMENT.md must be at the top of the tree')
if (existsSync(GUIDE)) {
    let guide = readFileSync(GUIDE, 'utf8')
    let header = readFileSync(join(ROOT, 'src', 'goahead.h'), 'utf8')

    let settings = new Set(guide.match(/ME_GOAHEAD_[A-Z0-9_]+/g) || [])
    ttrue(settings.size > 0, 'the guide names build settings')
    for (let setting of settings) {
        ttrue(header.includes(`#ifndef ${setting}`), `setting ${setting} named by the guide must be declared in goahead.h`)
    }
    let paths = new Set((guide.match(/`((?:src|doc|test|certs)\/[^`]*)`/g) || []).map(p => p.slice(1, -1)))
    ttrue(paths.size > 0, 'the guide names files')
    for (let path of paths) {
        //  A tree without doc/ cannot check doc/ paths
        if (path.startsWith('doc/') && !existsSync(join(ROOT, 'doc'))) {
            continue
        }
        ttrue(existsSync(join(ROOT, path)), `path ${path} named by the guide must exist`)
    }

    //  escapeUri leaves ' and & and javascript: intact, so the guide must not present it as enough on its own
    let escaping = guide.split('\n').find(line => line.includes('Escape request data in templates')) || ''
    ttrue(escaping != '', 'the guide advises on escaping request data in templates')
    ttrue(/escapeUri[^.]*\.[^]*writeHtml/.test(escaping) && escaping.includes('scheme'),
        'the guide must say escapeUri output still needs writeHtml, and that a URL needs its scheme checked')

    //  Each area an operator must configure has guidance, named by the setting or term an operator searches for
    const TOPICS: [string, RegExp][] = [
        ['TLS protocol versions', /SSL_NO_V1_2/],
        ['the CGI variable prefix', /CGI_VAR_PREFIX/],
        ['CGI query terms passed as program arguments', /command.line arguments/i],
        ['security response headers', /RESPONSE_HEADERS[^\n]*Content-Security-Policy/],
        ['password file permissions', /auth\.txt[^\n]*mode 0600/],
        ['MD5 hashes as Digest password equivalents', /MD5[^\n]*password equivalent/],
        ['gopass prompting instead of --password', /gopass[^\n]*without `--password`/],
        ['session lifetime and cookie flags', /LIMIT_SESSION_LIFE[^\n]*HttpOnly[^\n]*Secure/],
        ['stealth mode', /STEALTH[^\n]*TRACE/],
        ['upgrading legacy password hashes', /^## Upgrading[^]*BF1/m],
        ['building the library and application alike', /same settings/],
        ['the running-as-root warning of the goahead program only', /embedding GoAhead gets no such warning/],
    ]
    for (let [topic, pattern] of TOPICS) {
        ttrue(pattern.test(guide), `the guide must cover ${topic}`)
    }
}

//  The shipped man pages and sample route file must match the code
const MAN = join(ROOT, 'doc', 'man')
if (existsSync(MAN)) {
    let goahead = readFileSync(join(MAN, 'goahead.txt'), 'utf8')
    ttrue(/--log stdout:2\./.test(goahead), 'goahead.txt must describe --verbose as --log stdout:2')
    ttrue(/ME_GOAHEAD_AUTH_STORE="pam"/.test(goahead), 'goahead.txt must say PAM needs ME_GOAHEAD_AUTH_STORE')
    let gopass = readFileSync(join(MAN, 'gopass.txt'), 'utf8')
    ttrue(!/^ {10,}gopass [^\n]*--password/m.test(gopass),
        'gopass.txt examples must not put the password on the command line')
}
let routes = readFileSync(join(ROOT, 'src', 'route.txt'), 'utf8')
let schema = (routes.match(/Schema\n#\s+route [^\n]*\\\n#[^\n]*/) || [''])[0]
ttrue(/ auth=/.test(schema) && / dir=/.test(schema), 'the route.txt schema must list auth= and dir=')
ttrue(!/ME_GOAHEAD_PUT_DIR directory/.test(routes), 'route.txt must not say PUT writes into ME_GOAHEAD_PUT_DIR')

const BUILDLIB = join(ROOT, 'bin', 'buildLib.sh')
if (existsSync(BUILDLIB)) {
    tcontains(readFileSync(BUILDLIB, 'utf8'), 'SECURITY-DEPLOYMENT.md', 'the distribution build must stage the guide')
}
