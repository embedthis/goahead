/*
    cgi-vxworks-lifecycle.tst - The VxWorks CGI backend manages module, task and descriptor lifetimes
 */

import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tget, thas, tskip, ttrue } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync } from 'node:fs'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const cc = Cmd.locate('cc')
//  The fixture links the built library, so it compiles with the library's feature flags
const FEATURES = String(tget('TM_FEATURES_HEADER'))
if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX host compiler')
} else if (!thas('ME_GOAHEAD_CGI')) {
    tskip('CGI is not enabled')
} else {
    const root = join(import.meta.dir, '..', '..')
    const dir = mkdtempSync(join(tmpdir(), 'goahead-vxcgi-'))
    try {
        const exe = join(dir, 'vxcgi')
        const args = ['-include', FEATURES,
            '-DME_OS_TYPE=16', '-DVXWORKS=1', '-D_WRS_VXWORKS_MAJOR=7', '-D_WRS_VXWORKS_MINOR=0',
            '-Werror=int-conversion', '-Werror=incompatible-pointer-types', '-Werror=implicit-function-declaration',
            '-Werror=int-to-pointer-cast', '-Werror=pointer-to-int-cast',
            '-I' + join(import.meta.dir, 'vxworks'), '-I' + join(root, 'src'), '-I' + join(root, 'src/osdep'),
            join(import.meta.dir, 'cgi-vxworks-fixture.c'),
            '-L' + join(root, 'build/bin'), '-Wl,-rpath,' + join(root, 'build/bin'), '-lgoahead', '-o', exe]
        const built = spawnSync(String(cc), args, { encoding: 'utf8' })
        teq(built.status, 0, 'the VxWorks CGI backend compiles: ' + built.stderr)

        const result = spawnSync(exe, [], { encoding: 'utf8' })
        teq(result.status, 0, 'the VxWorks CGI lifecycle checks pass: ' + result.stdout + result.stderr)
        tcontains(result.stdout, 'module lifetime passed', 'a module stays loaded until its last task has gone')
        tcontains(result.stdout, 'task identity passed', 'a reused task ID is never taken for the CGI task')
        tcontains(result.stdout, 'aborted stdio passed', 'an aborted task releases its descriptors')
        tcontains(result.stdout, 'priority passed', 'the CGI task runs below the server')
        tcontains(result.stdout, 'image entry passed', 'an entry point in the image is neither loaded nor unloaded')
        tcontains(result.stdout, 'launch failure passed', 'a failed launch releases its module and descriptors')


        const source = readFileSync(join(root, 'src', 'cgi.c'), 'utf8')
        const start = source.indexOf('static void vxWebsCgiEntry(void *entryAddr(int argc, char **argv), char **argp')
        const end = source.indexOf('\n}\n', start)
        ttrue(start >= 0 && end > start, 'the VxWorks task wrapper is defined')
        const wrapper = source.slice(start, end)
        ttrue(!/\b(open|creat)\(/.test(wrapper), 'the task wrapper does not open its own stdio')
    } finally {
        rmSync(dir, { recursive: true, force: true })
    }
}
