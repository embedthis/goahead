/*
    A request path is refused unless it is in Unicode NFC: a code point that NFC replaces, or a mark composing with
    the starter before it, names the same file as its NFC spelling on a normalization-insensitive file system
    (APFS, HFS+) while routing compares the bytes. Companion of regress/uripath-utf8-canonical and uripath-casefold.
 */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/*
    Each row is a non-NFC spelling and the NFC spelling of the same name. The first is refused, the second accepted.
 */
static struct {
    cchar *nonCanonical;
    cchar *canonical;
    cchar *what;
} names[] = {
    { "\xE1\xBD\xB1",            "\xCE\xAC",               "U+1F71 alpha with oxia vs U+03AC with tonos" },
    { "\xE1\xBF\xBB",            "\xCE\x8F",               "U+1FFB omega with oxia vs U+038F with tonos" },
    { "\xE2\x84\xA6",            "\xCE\xA9",               "U+2126 Ohm vs U+03A9 Omega" },
    { "\xE2\x84\xAB",            "\xC3\x85",               "U+212B Angstrom vs U+00C5 A with ring" },
    { "\xCD\xBE",                ";",                      "U+037E Greek question mark vs semicolon" },
    { "\xE1\xBF\xAF",            "`",                      "U+1FEF Greek varia vs grave accent" },
    { "\xE2\x80\x80",            "\xE2\x80\x82",           "U+2000 en quad vs U+2002 en space" },
    { "\xEF\xA4\x80",            "\xE8\xB1\x88",           "U+F900 CJK compatibility vs U+8C48" },
    { "\xF0\xAF\xA0\x80",        "\xE4\xB8\xBD",           "U+2F800 CJK compatibility vs U+4E3D" },
    { "\xE0\xA5\x98",            "\xE0\xA4\x95\xE0\xA4\xBC", "U+0958 Devanagari qa vs ka + nukta" },
    { "\xE0\xA7\x9F",            "\xE0\xA6\xAF\xE0\xA6\xBC", "U+09DF Bengali yya vs ya + nukta (excluded from composition)" },
    { "\xE0\xA4\xA8\xE0\xA4\xBC", "\xE0\xA4\xA9",          "Devanagari na + nukta vs U+0929 nnna" },
    { "\xE0\xA7\x87\xE0\xA6\xBE", "\xE0\xA7\x8B",          "Bengali e + aa vs U+09CB o" },
    { "\xE0\xAF\x86\xE0\xAE\xBE", "\xE0\xAF\x8A",          "Tamil e + aa vs U+0BCA o" },
    { "\xE0\xBD\x83",            "\xE0\xBD\x82\xE0\xBE\xB7", "U+0F43 Tibetan gha vs ga + subjoined ha (excluded from composition)" },
    { "\xE1\x80\xA5\xE1\x80\xAE", "\xE1\x80\xA6",          "Myanmar u + ii vs U+1026 uu" },
    { "\xEF\xAC\x9D",            "\xD7\x99\xD6\xB4",       "U+FB1D Hebrew yod with hiriq (NFC keeps the pair; the mark is refused as decomposed)" },
};

/*
    Percent-encode the name as a path segment, as a client sends it
 */
static bool accepted(cchar *name)
{
    char  *uri, *path, *op;
    cchar *cp;

    uri = op = walloc(slen(name) * 3 + 16);
    op += slen(fmt(op, 6, "/dir/"));
    for (cp = name; *cp; cp++) {
        op += slen(fmt(op, 4, "%%%02X", (uchar) *cp));
    }
    scopy(op, 11, "/page.html");
    path = websValidateUriPath(uri);
    wfree(uri);
    wfree(path);
    return path != 0;
}

int main(int argc, char **argv)
{
    int i;

    for (i = 0; i < (int) (sizeof(names) / sizeof(names[0])); i++) {
        ttrue(!accepted(names[i].nonCanonical), "non-NFC spelling must be refused: %s", names[i].what);
    }
    /*
        The NFC spellings stay reachable, except where the project already refuses the composing mark itself
     */
    for (i = 0; i < (int) (sizeof(names) / sizeof(names[0])); i++) {
        if (strstr(names[i].what, "the mark is refused")) {
            continue;
        }
        ttrue(accepted(names[i].canonical), "NFC spelling must be accepted: %s", names[i].what);
    }
    /*
        A composing mark after a starter it does not compose with is ordinary NFC text
     */
    ttrue(accepted("\xE0\xA4\x95\xE0\xA4\xBC"), "Devanagari ka + nukta is NFC (U+0958 is excluded from composition)");
    ttrue(accepted("\xE0\xA6\x95\xE0\xA6\xBE"), "Bengali ka + vowel sign aa is NFC");
    ttrue(accepted("\xE0\xAE\x95\xE0\xAE\xBE"), "Tamil ka + vowel sign aa is NFC");
    ttrue(accepted("caf\xC3\xA9"), "U+00E9 is NFC");
    ttrue(accepted("\xEA\xB0\x80"), "Hangul syllable U+AC00 is NFC");
    return 0;
}
