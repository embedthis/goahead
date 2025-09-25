/*
    alloc.c -- Optional WebServer memory allocator

    This file implements a fast block allocation scheme suitable for operating systems whose malloc suffers from
    fragmentation.  This allocator maintains block class queues for rapid allocation and minimal fragmentation. This
    allocator does not coalesce blocks. The storage space may be populated statically or via the traditional malloc
    mechanisms. Large blocks greater than the maximum class size may be allocated from the O/S or run-time system via
    malloc. To permit the use of malloc, call wopen with flags set to WEBS_USE_MALLOC (this is the default).  It is
    recommended that wopen be called first thing in the application.  If it is not, it will be called with default
    values on the first call to walloc(). Note that this code is not designed for multi-threading purposes and it
    depends on newly declared variables being initialized to zero.

    You should ALWAYS configure a memory notifier and abort the process or perform other recover incase of
    memory exhaustion. You must set WEBS_DEFAULT_MEM to be sufficient memory for your needs.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/********************************* Includes ***********************************/

#include    "goahead.h"

/********************************** Locals ************************************/

static WebsMemNotifier memNotifier;

/*
    The memory allocator should ALWAYS be configured as GoAhead does not check every memory allocation.
 */
PUBLIC void websSetMemNotifier(WebsMemNotifier cback)
{
    memNotifier = cback;
}

static void defaultMemNotifier(ssize size)
{
    fprintf(stderr, "Cannot allocate required block of %ld bytes, terminating.", size);
    abort();
}

/********************************* Defines ************************************/
#if ME_GOAHEAD_REPLACE_MALLOC
/*
    ROUNDUP4(size) returns the next higher integer value of size that is divisible by 4, or the value of size if size is
    divisible by 4. ROUNDUP4() is used in aligning memory allocations on 4-byte boundaries.
    Note:  ROUNDUP4() is only required on some operating systems.
 */
#define ROUNDUP4(size) ((size) % 4) ? (size) + (4 - ((size) % 4)) : (size)

/*
    qhead blocks are created as the original memory allocation is freed up. See wfree.
 */
static WebsAlloc *qhead[WEBS_MAX_CLASS];                /* Per class block q head */
static char      *freeBuf;                              /* Pointer to free memory */
static char      *freeNext;                             /* Pointer to next free mem */
static int       freeSize;                              /* Size of free memory */
static int       freeLeft;                              /* Size of free left for use */
static int       controlFlags = WEBS_USE_MALLOC;        /* Default to auto-malloc */
static int       wopenCount = 0;                        /* Num tasks using walloc */

static int wallocGetSize(ssize size, int *q);

#endif /* ME_GOAHEAD_REPLACE_MALLOC */

/********************************** Code **************************************/
/*
    Initialize the walloc module. wopenAlloc should be called the very first thing after the application starts and
    wcloseAlloc should be called the last thing before exiting. If wopenAlloc is not called, it will be called on the
       first
    allocation with default values. "buf" points to memory to use of size "bufsize". If buf is NULL, memory is allocated
    using malloc. flags may be set to WEBS_USE_MALLOC if using malloc is okay. This routine will allocate *  an initial
    buffer of size bufsize for use by the application.
 */
PUBLIC int wopenAlloc(void *buf, int bufsize, int flags)
{
#if ME_GOAHEAD_REPLACE_MALLOC
    controlFlags = flags;

    /*
        If wopen already called by a shared process, just increment the count and return
     */
    if (++wopenCount > 1) {
        return 0;
    }
    if (buf == NULL) {
        if (bufsize == 0) {
            bufsize = WEBS_DEFAULT_MEM;
        }
        bufsize = ROUNDUP4(bufsize);
        if ((buf = malloc(bufsize)) == NULL) {
            /*
                Resetting wopenCount so client code can decide to call wopenAlloc() again with a smaller memory request.
             */
            --wopenCount;
            return -1;
        }
    } else {
        controlFlags |= WEBS_USER_BUF;
    }
    freeSize = freeLeft = bufsize;
    freeBuf = freeNext = buf;
    memset(qhead, 0, sizeof(qhead));

#endif /* ME_GOAHEAD_REPLACE_MALLOC */
    if (!memNotifier) {
        websSetMemNotifier(defaultMemNotifier);
    }
    return 0;
}


PUBLIC void wcloseAlloc(void)
{
#if ME_GOAHEAD_REPLACE_MALLOC
    if (--wopenCount <= 0 && !(controlFlags & WEBS_USER_BUF)) {
        free(freeBuf);
        wopenCount = 0;
    }
#endif /* ME_GOAHEAD_REPLACE_MALLOC */
}


#if ME_GOAHEAD_REPLACE_MALLOC
/*
    Allocate a block of the requested size. First check the block queues for a suitable one.
 */
PUBLIC void *walloc(ssize size)
{
    WebsAlloc *bp;
    int       q, memSize;

    /*
        Call wopen with default values if the application has not yet done so
     */
    if (freeBuf == NULL) {
        if (wopenAlloc(NULL, WEBS_DEFAULT_MEM, 0) < 0) {
            return NULL;
        }
    }
    if (size < 0) {
        return NULL;
    }
    memSize = wallocGetSize(size, &q);

    if (q >= WEBS_MAX_CLASS) {
        /*
            Size if bigger than the maximum class. Malloc if use has been okayed
         */
        if (controlFlags & WEBS_USE_MALLOC) {
            memSize = ROUNDUP4(memSize);
            bp = (WebsAlloc*) malloc(memSize);
            if (bp == NULL) {
                if (memNotifier) {
                    (memNotifier) (memSize);
                }
                return NULL;
            }
        } else {
            if (memNotifier) {
                (memNotifier) (memSize);
            }
            return NULL;
        }
        /*
            the u.size is the actual size allocated for data
         */
        bp->u.size = memSize - sizeof(WebsAlloc);
        bp->flags = WEBS_MALLOCED;

    } else if ((bp = qhead[q]) != NULL) {
        /*
            Take first block off the relevant q if non-empty
         */
        qhead[q] = bp->u.next;
        bp->u.size = memSize - sizeof(WebsAlloc);
        bp->flags = 0;

    } else {
        if (freeLeft > memSize) {
            /*
                The q was empty, and the free list has spare memory so create a new block out of the primary free block
             */
            bp = (WebsAlloc*) freeNext;
            freeNext += memSize;
            freeLeft -= memSize;
            bp->u.size = memSize - sizeof(WebsAlloc);
            bp->flags = 0;

        } else if (controlFlags & WEBS_USE_MALLOC) {
            /*
                Nothing left on the primary free list, so malloc a new block
             */
            memSize = ROUNDUP4(memSize);
            if ((bp = (WebsAlloc*) malloc(memSize)) == NULL) {
                if (memNotifier) {
                    (memNotifier) (memSize);
                }
                return NULL;
            }
            bp->u.size = memSize - sizeof(WebsAlloc);
            bp->flags = WEBS_MALLOCED;

        } else {
            if (memNotifier) {
                (memNotifier) (memSize);
            }
            return NULL;
        }
    }
    bp->flags |= WEBS_INTEGRITY;
    return (void*) ((char*) bp + sizeof(WebsAlloc));
}


/*
    Free a block back to the relevant free q. We don't free back to the O/S or run time system unless the block is
    greater than the maximum class size. We also do not coalesce blocks.
 */
PUBLIC void wfree(void *mp)
{
    WebsAlloc *bp;
    int       q;

    if (mp == 0) {
        return;
    }
    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert((bp->flags & WEBS_INTEGRITY_MASK) == WEBS_INTEGRITY);
    if ((bp->flags & WEBS_INTEGRITY_MASK) != WEBS_INTEGRITY) {
        return;
    }
    wallocGetSize(bp->u.size, &q);
    if (bp->flags & WEBS_MALLOCED) {
        free(bp);
        return;
    }
    /*
        Simply link onto the head of the relevant q
     */
    bp->u.next = qhead[q];
    qhead[q] = bp;
    bp->flags = WEBS_FILL_WORD;
}


/*
    Reallocate a block. Allow NULL pointers and just do a malloc. Note: if the realloc fails, we return NULL and the
    previous buffer is preserved.
 */
PUBLIC void *wrealloc(void *mp, ssize newsize)
{
    WebsAlloc *bp;
    void      *newbuf;

    if (mp == NULL) {
        return walloc(newsize);
    }
    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert((bp->flags & WEBS_INTEGRITY_MASK) == WEBS_INTEGRITY);

    /*
        If the allocated memory already has enough room just return the previously allocated address.
     */
    if (bp->u.size >= newsize) {
        return mp;
    }
    if ((newbuf = walloc(newsize)) != NULL) {
        memcpy(newbuf, mp, bp->u.size);
        wfree(mp);
    }
    return newbuf;
}


/*
    Find the size of the block to be walloc'ed.  It takes in a size, finds the smallest binary block it fits into, adds
    an overhead amount and returns.  q is the binary size used to keep track of block sizes in use.  Called from both
    walloc and wfree.
 */
static int wallocGetSize(ssize size, int *q)
{
    ssize mask;

    mask = (size == 0) ? 0 : (size - 1) >> WEBS_SHIFT;
    for (*q = 0; mask; mask >>= 1) {
        *q = *q + 1;
    }
    return (1 << (WEBS_SHIFT + *q)) + sizeof(WebsAlloc);
}


PUBLIC void wvalid(cvoid *mp)
{
    WebsAlloc *bp;

    bp = (WebsAlloc*) ((char*) mp - sizeof(WebsAlloc));
    assert(bp->flags & WEBS_INTEGRITY);
}

#else /* !ME_GOAHEAD_REPLACE_MALLOC */

PUBLIC void *walloc(ssize num)
{
    void *mem;

    if ((mem = malloc(num)) == 0) {
        if (memNotifier) {
            (memNotifier) (num);
        }
    }
    return mem;
}


PUBLIC void wfree(void *mem)
{
    if (mem) {
        free(mem);
    }
}


PUBLIC void *wrealloc(void *mem, ssize num)
{
    void *old;

    old = mem;
    if ((mem = realloc(mem, num)) == 0) {
        if (memNotifier) {
            (memNotifier) (num);
        }
        free(old);
    }
    return mem;
}
#endif /* ME_GOAHEAD_REPLACE_MALLOC */


PUBLIC void *wdup(cvoid *ptr, ssize usize)
{
    char *newp;

    if ((newp = walloc(usize)) != 0) {
        memcpy(newp, ptr, usize);
    }
    return newp;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
