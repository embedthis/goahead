/* websFreeUpload must clear the files hash it frees. */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_UPLOAD

static Webs *allocWebs(void)
{
    Webs *wp;

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->files = hashCreate(7);
    wp->upfd = -1;
    return wp;
}


static void suiteA_cleared(void)
{
    Webs *wp;

    tinfo("\n--- Suite A: the files handle is cleared ---\n");

    wp = allocWebs();
    websFreeUpload(wp);
    ttrue(wp->files == -1, "websFreeUpload must clear the files handle it frees");
    wfree(wp);
}


static void suiteB_twice(void)
{
    Webs     *wp;
    WebsHash other;

    tinfo("\n--- Suite B: a second free leaves other tables alone ---\n");

    wp = allocWebs();
    websFreeUpload(wp);
    other = hashCreate(7);
    hashEnter(other, "key", valueString("value", VALUE_ALLOCATE), 0);
    websFreeUpload(wp);
    ttrue(hashLookup(other, "key") != 0, "a table created after the first free must survive the second");
    hashFree(other);
    wfree(wp);
}
#endif


int main(int argc, char **argv)
{
    tinfo("upload-free-idempotent: websFreeUpload must clear the files hash it frees\n");
#if ME_GOAHEAD_UPLOAD
    suiteA_cleared();
    suiteB_twice();
#endif
    return 0;
}
