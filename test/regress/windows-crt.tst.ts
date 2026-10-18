/*
    windows-crt.tst - The Windows library, programs and tests share one C runtime
 */
import { tcontains, ttrue } from '@embedthis/testme'
import { readdirSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')
const VS = join(ROOT, 'projects', 'vs2022')

let projects = readdirSync(VS).filter(f => f.endsWith('.vcxproj'))
ttrue(projects.length > 0, 'the Visual Studio projects are present')

for (let project of projects) {
    let text = readFileSync(join(VS, project), 'utf8')
    let runtimes = [...text.matchAll(/<RuntimeLibrary>([^<]*)<\/RuntimeLibrary>/g)].map(m => m[1])
    ttrue(runtimes.length > 0, `${project} names its C runtime`)
    for (let runtime of runtimes) {
        ttrue(runtime == 'MultiThreadedDLL' || runtime == 'MultiThreadedDebugDLL',
              `${project} must use the dynamic C runtime, got ${runtime}`)
    }
    ttrue(!/\\MTd?[;<]/.test(text), `${project} must not link libraries built for the static runtime`)
}

let config = readFileSync(join(ROOT, 'test', 'testme.json5'), 'utf8')
tcontains(config, "'-MDd'", 'the tests must compile with the dynamic debug runtime the debug library uses')
