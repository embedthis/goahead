/*
    auth-flag-fail-closed.tst - Regression test for the ME_GOAHEAD_AUTH=0 fail-open and its siblings.
 */

import { Cmd } from '@embedthis/ejscript'
import { tskip, ttrue } from '@embedthis/testme'
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'
import { cc, compile, isWindows, ROOT } from '../utils/amalgamation'

/*
    Check configuration loading, startup route auditing, and public-API route construction independently.
 */
const DRIVER = `
#include "goahead.h"

static int apiProbe(void)
{
    WebsRoute *route;
    WebsHash  abilities;

    if ((route = websAddRoute("/private", "file", -1)) == 0) {
        return -1;
    }
    abilities = hashCreate(-1);
    hashEnter(abilities, "manage", valueInteger(0), 0);
    return websSetRouteMatch(route, 0, 0, -1, -1, abilities, -1);
}

int main(int argc, char **argv)
{
    int rc;

    // The tables below put a protected route ahead of the catch-all, which strict mode refuses
    websSetStrict(0);
    if (websOpen(argv[2], argc > 3 ? argv[3] : 0) < 0) {
        return 1;
    }
    rc = smatch(argv[1], "api") ? apiProbe() : smatch(argv[1], "check") ? -websCheckRoutes() : 0;
    websClose();
    return rc < 0 ? 1 : 0;
}
`

/*
    A route table with no access control at all. Every case below appends its own directive to
    this base, so a rejection can only come from the directive under test.
 */
const BASE_ROUTES = 'route uri=/ handler=file\n'

interface Probe {
    name: string
    routes: string
    /* Must the load be refused? */
    reject: boolean
}

const PROBES: Probe[] = [
    {name: 'auth= directive', routes: 'route uri=/private auth=basic handler=file\n' + BASE_ROUTES, reject: true},
    {name: 'abilities= directive', routes: 'route uri=/private abilities=manage handler=file\n' + BASE_ROUTES,
        reject: true},
    {name: 'user directive', routes: BASE_ROUTES + 'user name=joshua password=x roles=manager\n', reject: true},
    {name: 'role directive', routes: BASE_ROUTES + 'role name=manager abilities=manage\n', reject: true},
    /* Control - a route table that asks for nothing this build cannot do must still load */
    {name: 'route table without access control', routes: BASE_ROUTES, reject: false},
]

/*
    Probes for a build that has authentication. Each records what the shipped default must do, so
    suite B is a real expectation rather than "everything suite A rejected is accepted here".
 */
const AUTH_PROBES: Probe[] = [
    {name: 'auth=form route', routes: 'route uri=/private auth=form handler=continue abilities=manage\n' +
        BASE_ROUTES, reject: false},
    {name: 'auth=digest route', routes: 'route uri=/private auth=digest handler=file\n' + BASE_ROUTES,
        reject: false},
    /* A typo used to install a route that could never issue a challenge and denied every request */
    {name: 'unknown auth type', routes: 'route uri=/private auth=bsaic handler=file\n' + BASE_ROUTES,
        reject: true},
]

if (isWindows) {

    tskip('Rebuilding the amalgamation is a POSIX-only arm of this test')
} else if (!cc) {
    tskip('No C compiler on PATH to rebuild the amalgamation at ME_GOAHEAD_AUTH=0')
} else {
    let dir = mkdtempSync(join(tmpdir(), 'goahead-authflag-'))
    let web = join(dir, 'web')
    mkdirSync(web)
    writeFileSync(join(dir, 'driver.c'), DRIVER)
    //  The amalgamation is built without PAM, which is what suite D needs

    /*
        Build one program per flag setting. Returns the executable path, or null if the build failed.
     */
    function build(label: string, defines: string, main: string): string | null {
        let exe = join(dir, 'probe-' + label)
        let cmd = compile(exe, defines, main)
        if (cmd.status != 0) {
            ttrue(false, 'Cannot build the ' + label + ' probe:\n' + String(cmd.error))
            return null
        }
        return exe
    }

    function buildDriver(label: string, defines: string): string | null {
        return build(label, defines, join(dir, 'driver.c'))
    }


    function buildFails(label: string, defines: string): string | null {
        let cmd = compile(join(dir, 'probe-' + label), defines, join(dir, 'driver.c'))
        return cmd.status != 0 ? String(cmd.error) + String(cmd.response) : null
    }

    /*
        Run a driver over a route table and report whether the load was accepted. The driver runs
        with the temporary directory as its working directory because the route file path, the
        documents directory and the default staging directories are all resolved relative to it.
     */
    function load(exe: string, routes: string, mode = 'load'): {accepted: boolean, output: string} {
        writeFileSync(join(dir, 'route.txt'), routes)
        let cmd = new Cmd(exe + ' ' + mode + ' web route.txt', {dir: dir})
        return {accepted: cmd.status == 0, output: String(cmd.error) + String(cmd.response)}
    }

    /*
        Run one probe set against one driver. `flag` names the build setting for the messages, and
        `names` is the setting the refusal must mention so the operator can act on it.
     */
    function check(exe: string, probes: Probe[], flag: string, names: RegExp) {
        for (let probe of probes) {
            let r = load(exe, probe.routes)
            if (probe.reject) {
                ttrue(!r.accepted, flag + ' must refuse to start with an ' + probe.name +
                    ' rather than serve the route without the access control it describes:\n' + r.output)
                ttrue(names.test(r.output), 'The refusal for an ' + probe.name + ' under ' + flag +
                    ' must name the setting at fault:\n' + r.output)
            } else {
                ttrue(r.accepted, flag + ' must accept an ' + probe.name + ':\n' + r.output)
            }
        }
    }

    /*
        Suite A - ME_GOAHEAD_AUTH=0 refuses a route table it cannot enforce, whether the requirement
        arrives through a route file or through the public API.
     */
    let noauth = buildDriver('noauth', '-DME_GOAHEAD_AUTH=0')
    if (noauth) {
        check(noauth, PROBES, 'ME_GOAHEAD_AUTH=0', /ME_GOAHEAD_AUTH/)

        let api = new Cmd(noauth + ' api web', {dir: dir})
        ttrue(api.status != 0, 'ME_GOAHEAD_AUTH=0 must refuse an ability requirement passed directly to ' +
            'websSetRouteMatch, not only one parsed from a route file:\n' + String(api.error))
        ttrue(/ME_GOAHEAD_AUTH/.test(String(api.error) + String(api.response)),
            'The websSetRouteMatch refusal must name ME_GOAHEAD_AUTH:\n' + String(api.error))


        let r = load(noauth, 'route uri=/cgi-bin/ handler=cgi\n' + BASE_ROUTES, 'check')
        ttrue(r.accepted, 'ME_GOAHEAD_AUTH=0 must warn about, not refuse, a table that runs code:\n' + r.output)
        ttrue(/ME_GOAHEAD_AUTH=0[^\n]*unauthenticated[^\n]*"\/cgi-bin\/" \(handler=cgi\)/.test(r.output),
            'ME_GOAHEAD_AUTH=0 must warn that every route is unauthenticated, naming the CGI route:\n' + r.output)
        r = load(noauth, BASE_ROUTES, 'check')
        ttrue(r.accepted && /ME_GOAHEAD_AUTH=0[^\n]*unauthenticated clients\./.test(r.output),
            'ME_GOAHEAD_AUTH=0 must warn that a static table is served to unauthenticated clients:\n' + r.output)
    }

    /*
        Suite B - the shipped default. Everything suite A refused loads here, so suite A is measuring
        the build flag and not a broken driver, and a typo in auth= is still refused.
     */
    let dflt = buildDriver('default', '-DME_GOAHEAD_AUTH=1')
    if (dflt) {
        check(dflt, PROBES.map((p) => ({...p, reject: false})), 'ME_GOAHEAD_AUTH=1', /./)
        check(dflt, AUTH_PROBES, 'ME_GOAHEAD_AUTH=1', /authentication type/)

        let api = new Cmd(dflt + ' api web', {dir: dir})
        ttrue(api.status == 0, 'ME_GOAHEAD_AUTH=1 must accept an ability requirement passed to ' +
            'websSetRouteMatch:\n' + String(api.error))
    }

    /*
        Suite C - the same rule for the digest sub-feature
     */
    let nodigest = buildDriver('nodigest', '-DME_GOAHEAD_AUTH=1 -DME_GOAHEAD_DIGEST=0')
    if (nodigest) {
        check(nodigest, AUTH_PROBES.map((p) => ({...p, reject: p.reject || p.name.includes('digest')})),
            'ME_GOAHEAD_DIGEST=0', /ME_GOAHEAD_DIGEST|authentication type/)
    }

    /*
        Suite D - the password store. websOpenAuth runs before any route is loaded, so a store that
        is not compiled in must stop the server there. The route table is the innocent one from
        BASE_ROUTES, so the refusal can only come from the store.

        "pam" is the case that occurs in practice - PAM is a host dependency, not a build flag the
        operator sets - and an unrecognized name covers a typo in the same setting.
     */
    const STORES = [
        {name: 'pam', label: 'pamstore'},
        {name: 'bogus', label: 'unknownstore'},
    ]
    for (let store of STORES) {
        let exe = buildDriver(store.label, '-DME_GOAHEAD_AUTH_STORE=\\"' + store.name + '\\"')
        if (!exe) continue
        let r = load(exe, BASE_ROUTES)
        ttrue(!r.accepted, 'A build whose ME_GOAHEAD_AUTH_STORE="' + store.name + '" is not compiled in ' +
            'must refuse to start rather than fall back to the file store:\n' + r.output)
        ttrue(/ME_GOAHEAD_AUTH_STORE/.test(r.output),
            'The refusal must name ME_GOAHEAD_AUTH_STORE:\n' + r.output)
    }

    /*
        Suite E - auto-login. It is not fatal: it is a development switch, and refusing to start
        would delete the feature rather than fix it. The warning is therefore the entire guarantee,
        and a warning nobody asserts is one that can silently stop being emitted.
     */
    let autologin = buildDriver('autologin', '-DME_GOAHEAD_AUTO_LOGIN=1')
    if (autologin) {
        let r = load(autologin, BASE_ROUTES)
        ttrue(r.accepted, 'ME_GOAHEAD_AUTO_LOGIN=1 must still start - it is a development switch, ' +
            'not a configuration error:\n' + r.output)
        ttrue(/ME_GOAHEAD_AUTO_LOGIN/.test(r.output), 'ME_GOAHEAD_AUTO_LOGIN=1 must warn at the default ' +
            'log level, naming the setting:\n' + r.output)
    }

    let silent = buildFails('autologin-nolog', '-DME_GOAHEAD_AUTO_LOGIN=1 -DME_GOAHEAD_LOGGING=0')
    ttrue(silent != null, 'ME_GOAHEAD_AUTO_LOGIN=1 with ME_GOAHEAD_LOGGING=0 must not compile, because ' +
        'it would serve protected routes without credentials and without a warning')
    if (silent != null) {
        ttrue(/ME_GOAHEAD_AUTO_LOGIN/.test(silent) && /ME_GOAHEAD_LOGGING/.test(silent),
            'The compile error must name ME_GOAHEAD_AUTO_LOGIN and ME_GOAHEAD_LOGGING:\n' + silent)
    }
    if (dflt) {
        /* Control - the default build must not cry wolf */
        let r = load(dflt, BASE_ROUTES)
        ttrue(!/ME_GOAHEAD_AUTO_LOGIN/.test(r.output),
            'The default build must not emit the auto-login warning:\n' + r.output)
    }

    /*
        Suite F - the --auth option surface. This one lives in the shipped main program rather than
        the library, so it builds src/goahead.c. A build that ignores the auth file must not offer
        the option that names it, and must not swallow it either.
     */
    const MAIN = join(ROOT, 'src', 'goahead.c')

    let cliAuth = build('cli-auth', '-DME_GOAHEAD_AUTH=1', MAIN)
    if (cliAuth) {
        /* An unknown option prints usage and exits, so the option list can be read without starting a server */
        let usage = new Cmd(cliAuth + ' --unknown-option', {dir: dir})
        ttrue(/--auth/.test(String(usage.error) + String(usage.response)),
            'A build with authentication must offer --auth:\n' + String(usage.error))
    }

    let cliNoAuth = build('cli-noauth', '-DME_GOAHEAD_AUTH=0', MAIN)
    if (cliNoAuth) {
        let usage = new Cmd(cliNoAuth + ' --unknown-option', {dir: dir})
        ttrue(!/--auth/.test(String(usage.error) + String(usage.response)),
            'A build with ME_GOAHEAD_AUTH=0 must not offer --auth:\n' + String(usage.error))

        /*
            The defect itself: --auth used to be accepted and the file silently ignored, so the
            operator was told nothing while the users they configured went unread.

            The trailing --version is load-bearing. A build that accepts --auth consumes the
            filename and runs on to serve requests, so without a second option that exits, this
            probe would hang the suite rather than fail it - which is exactly what the pre-fix
            binary does. With it, both outcomes terminate and are told apart by which one happened:
            a rejecting build prints usage and exits non-zero before reaching --version, and an
            accepting build swallows the filename and exits zero having printed the version.
         */
        let used = new Cmd(cliNoAuth + ' --auth auth.txt --version', {dir: dir})
        ttrue(used.status != 0 && /Usage:/.test(String(used.error)),
            'ME_GOAHEAD_AUTH=0 must reject --auth with usage rather than accept it and ignore the ' +
            'file:\n' + String(used.error) + String(used.response))
    }

    rmSync(dir, {recursive: true, force: true})
}
