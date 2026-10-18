/*
    hash-null-name.tst.c - hashEnter rejects null and empty names
 */

#include "testme.h"
#include "goahead.h"

int main(int argc, char **argv)
{
    WebsHash hash;
    WebsKey  *kp;

    tinfo("hash-null-name: hashEnter must refuse a null or empty name\n");

    hash = hashCreate(7);
    ttrue(hash >= 0, "hashCreate must succeed");

    ttrue(hashEnter(hash, NULL, valueInteger(1), 0) == 0, "a null name must be refused");
    ttrue(hashEnter(hash, "", valueInteger(1), 0) == 0, "an empty name must be refused");
    ttrue(hashLookup(hash, "") == 0, "nothing may have been stored under an empty name");

    kp = hashEnter(hash, "session", valueInteger(42), 0);
    ttrue(kp != 0, "a valid name must still be entered");
    kp = hashLookup(hash, "session");
    ttrue(kp != 0 && kp->content.value.integer == 42, "a valid name must round-trip");

    hashFree(hash);
    return 0;
}
