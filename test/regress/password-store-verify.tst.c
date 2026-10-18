/* A password verifier installed before websOpenAuth survives it */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_AUTH

static bool refuseAll(Webs *wp)
{
    return 0;
}

int main(int argc, char **argv)
{
    websSetPasswordStoreVerify(refuseAll);
    teq(websOpenAuth(1), 0, "open authentication");
    ttrue(websGetPasswordStoreVerify() == refuseAll, "a verifier installed before websOpenAuth must survive it");
    websCloseAuth();

    teq(websOpenAuth(1), 0, "reopen authentication");
    ttrue(websGetPasswordStoreVerify() != refuseAll && websGetPasswordStoreVerify() != 0,
          "after websCloseAuth the configured store must be selected again");
    websCloseAuth();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Authentication is not enabled");
    return 0;
}
#endif
