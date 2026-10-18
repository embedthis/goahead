/*
    heap-fixture.h - Heap in use, for C tests that bound a leak in-process
 */

#ifndef _h_HEAP_FIXTURE
#define _h_HEAP_FIXTURE 1

#if MACOSX
    #include <malloc/malloc.h>
    #define HEAP_MEASURABLE 1
#elif defined(__GLIBC__) && (__GLIBC__ > 2 || __GLIBC_MINOR__ >= 33)
    #include <malloc.h>
    #define HEAP_MEASURABLE 1
#else
    #define HEAP_MEASURABLE 0
#endif

#if HEAP_MEASURABLE
static size_t heapInUse(void)
{
#if MACOSX
    malloc_statistics_t stats;

    malloc_zone_statistics(NULL, &stats);
    return stats.size_in_use;
#else
    return mallinfo2().uordblks;
#endif
}

typedef void (*HeapCampaign)(void *arg);

/* Measure the smallest heap growth across runs to exclude one-time allocator expansion. */
static ssize heapGrowth(HeapCampaign campaign, void *arg, int runs)
{
    size_t before;
    ssize  growth, least;
    int    i;

    least = MAXSSIZE;
    for (i = 0; i < runs; i++) {
        before = heapInUse();
        campaign(arg);
        growth = (ssize) (heapInUse() - before);
        least = min(least, growth);
    }
    return least;
}
#endif

#endif /* _h_HEAP_FIXTURE */
