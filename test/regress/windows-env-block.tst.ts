/* Compile the actual Windows-only serializer on every host, without Windows process APIs. */
import { Cmd, Config } from '@embedthis/ejscript'
import { teq, ttrue } from '@embedthis/testme'
import { readFileSync, writeFileSync, mkdtempSync, rmSync } from 'node:fs'
import { spawnSync } from 'node:child_process'
import { join } from 'node:path'
import { tmpdir } from 'node:os'

const source = readFileSync(join(import.meta.dir, '../../src/cgi.c'), 'utf8')
const start = source.indexOf('static uchar *tableToBlock(char **table)')
ttrue(start >= 0, 'production environment serializer located')
const end = source.indexOf('\n}', start) + 2
const windows = String(Config.OS).startsWith('win')
const cc = Cmd.locate(windows ? 'cl' : 'cc')
ttrue(!!cc, 'C compiler available')
const dir = mkdtempSync(join(tmpdir(), 'goahead-env-block-'))
try {
    const file = join(dir, 'probe.c'), exe = join(dir, windows ? 'probe.exe' : 'probe')
    writeFileSync(file, `#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef unsigned char uchar;
static size_t allocated;
static void *allocate(size_t size) { void *p = malloc(size); allocated = size; if (p) memset(p, 0xA5, size); return p; }
#define walloc allocate
` + source.slice(start, end) + `
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed: %s\\n", #x); return 1; } } while (0)
int main(void) {
    char *empty[] = { NULL }, *entries[] = { "A=one", "B=two", NULL };
    uchar *block = tableToBlock(empty);
    CHECK(block != NULL && allocated >= 2);
    CHECK(block[0] == 0 && block[1] == 0);
    free(block);
    block = tableToBlock(entries);
    CHECK(block != NULL && allocated >= 13);
    CHECK(memcmp(block, "A=one\\0B=two\\0\\0", 13) == 0);
    free(block);
    return 0;
}
`)
    const flags = windows ? ['/nologo', file, '/Fe' + exe, '/Fo' + join(dir, 'probe.obj')]
        : ['-Wall', '-Wextra', '-Werror', file, '-o', exe]
    const built = spawnSync(String(cc), flags, { encoding: 'utf8' })
    teq(built.status, 0, 'production serializer compiles: ' + built.stderr)
    const run = spawnSync(exe, [], { encoding: 'utf8' })
    teq(run.status, 0, 'empty and populated blocks are double terminated: ' + run.stderr)
} finally {
    rmSync(dir, { recursive: true, force: true })
}
