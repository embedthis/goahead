/*
    The replacement allocator aligns a caller-supplied arena. wopenAlloc documents no alignment for the buffer, and a
    plain char array may start on any byte, so the allocator must align the arena itself or every block header and
    every block it returns is misaligned: undefined behaviour, and a fault on cores that trap unaligned access.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef ME_GOAHEAD_REPLACE_MALLOC
#define ME_GOAHEAD_REPLACE_MALLOC 1
#include "goahead.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/alloc.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static char arena[16 * 1024];

int main(int argc, char **argv)
{
    char   *p, *q;
    int    i;

    //  Start the arena one byte past an aligned address, as a char array inside a struct or after a flag can
    teq(wopenAlloc(arena + 1, (int) sizeof(arena) - 1, 0), 0, "a caller-supplied arena is accepted");
    for (i = 0; i < 8; i++) {
        p = walloc(24 + i);
        ttrue(p != NULL, "a block is allocated from the arena");
        ttrue(((uintptr_t) p % 8) == 0, "a block from a caller-supplied arena is aligned for any object");
        *(double*) p = 1.5;
        *(int64*) (p + 8) = 7;
        q = wrealloc(p, 100);
        ttrue(((uintptr_t) q % 8) == 0, "a reallocated block from the arena is aligned");
        ttrue(*(double*) q == 1.5 && *(int64*) (q + 8) == 7, "reallocation keeps the content");
        wfree(q);
    }
    ttrue(freeNext >= arena && freeNext <= arena + sizeof(arena), "the arena is still the caller's buffer");
    return 0;
}
