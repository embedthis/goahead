/* Contradictory TLS settings must fail to build with a message naming the conflict */
import { Cmd, Config } from '@embedthis/ejscript'
import { tcontains, teq, tskip } from '@embedthis/testme'
import { spawnSync } from 'node:child_process'
import { join } from 'node:path'

const cc = Cmd.locate('cc')

const CASES = [
    {
        name: 'SECURE_ONLY without TLS',
        defines: ['-DME_GOAHEAD_SECURE_ONLY=1', '-DME_COM_SSL=0'],
        error: 'ME_GOAHEAD_SECURE_ONLY requires TLS (ME_COM_SSL)',
    },
    {
        name: 'every TLS version disabled',
        defines: ['-DME_GOAHEAD_SSL_NO_V1_2=1', '-DME_GOAHEAD_SSL_NO_V1_3=1'],
        error: 'ME_GOAHEAD_SSL_NO_V1_2 and ME_GOAHEAD_SSL_NO_V1_3 disable every TLS version',
    },
]

if (String(Config.OS).startsWith('win') || !cc) {
    tskip('Requires a POSIX compiler')
} else {
    const src = join(import.meta.dir, '..', '..', 'src')

    function preprocess(defines: string[]) {
        return spawnSync(String(cc), ['-E', '-I' + src, '-I' + join(src, 'osdep'), ...defines,
            join(src, 'goahead.h'), '-o', '/dev/null'], { encoding: 'utf8' })
    }

    for (let { name, defines, error } of CASES) {
        let result = preprocess(defines)
        teq(result.status != 0, true, `${name} must fail to build`)
        tcontains(result.stderr, error, `${name} must name the conflict`)
    }
    let control = preprocess(['-DME_GOAHEAD_SECURE_ONLY=1', '-DME_GOAHEAD_SSL_NO_V1_2=0'])
    teq(control.status, 0, `a consistent configuration must build: ${control.stderr}`)
}
