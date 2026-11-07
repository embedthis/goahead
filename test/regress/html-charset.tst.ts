/*
    html-charset.tst - Served HTML declares its character encoding

    GoAhead types a template or static page as text/html with no charset parameter, and leaves the
    encoding to the document. A page that declares none is decoded by the browser's own detection,
    which can be steered by request data reflected into the page, even when that data is escaped for
    its context. The shipped default page must therefore declare its encoding, and the deployment
    guide's template-escaping paragraph must tell the integrator to do the same in every page.
 */
import { tcontains, ttrue } from '@embedthis/testme'
import { readFileSync } from 'node:fs'
import { join } from 'node:path'

const ROOT = join(import.meta.dir, '..', '..')

let index = readFileSync(join(ROOT, 'src', 'web', 'index.html'), 'utf8')
ttrue(/<meta\s+charset\s*=\s*["']?utf-8["']?/i.test(index),
    'the shipped default page must declare its encoding with <meta charset="utf-8">')

let guide = readFileSync(join(ROOT, 'SECURITY-DEPLOYMENT.md'), 'utf8')
let paragraph = guide.split('\n').find(line => line.startsWith('- **Escape request data in templates')) || ''
ttrue(paragraph.length > 0, 'the deployment guide must keep its template-escaping paragraph')
tcontains(paragraph, 'meta charset', 'the escaping paragraph must tell integrators to declare the page encoding')
