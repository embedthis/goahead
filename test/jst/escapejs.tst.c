/* websEscapeJs escapes for a quoted JavaScript string */
#include "testme.h"
#include "goahead.h"

static void check(cchar *input, cchar *expect)
{
    char *result;

    result = websEscapeJs(input);
    tmatch(result, expect, "escapeJs of \"%s\"", input ? input : "(null)");
    wfree(result);
}

int main(int argc, char **argv)
{
    check(NULL, "");
    check("", "");
    check("abc XYZ 019,._-", "abc XYZ 019,._-");
    check("'", "\\u0027");
    check("\"", "\\u0022");
    check("\\", "\\u005C");
    check("</script>", "\\u003C\\u002Fscript\\u003E");
    check("a\r\nb", "a\\u000D\\u000Ab");
    check("\x7f", "\\u007F");
    //  UTF-8 passes through except the line and paragraph separators
    check("caf\xc3\xa9", "caf\xc3\xa9");
    check("\xe2\x80\xa8\xe2\x80\xa9", "\\u2028\\u2029");
    check("\xe2\x80\xa6", "\xe2\x80\xa6");
    return 0;
}
