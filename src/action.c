/*
    action.c -- Action handler

    This module implements the /action handler. It is a simple binding of URIs to C functions.
    This enables a very high performance implementation with easy parsing and decoding of query
    strings and posted data.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

/************************************ Locals **********************************/

static WebsHash actionTable = -1;            /* Symbol table for actions */

/************************************* Code ***********************************/
/*
    Process an action request. Returns 1 always to indicate it handled the URL
    Return true to indicate the request was handled, even for errors.
 */
static bool actionHandler(Webs *wp)
{
    WebsKey     *sp;
    char        actionBuf[ME_GOAHEAD_LIMIT_URI + 1];
    char        *cp, *actionName;
    WebsAction  fn;

    assert(websValid(wp));
    assert(actionTable >= 0);

    /*
        Extract the action name
     */
    scopy(actionBuf, sizeof(actionBuf), wp->path);
    if ((actionName = strchr(&actionBuf[1], '/')) == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing action name");
        return 1;
    }
    actionName++;
    if ((cp = strchr(actionName, '/')) != NULL) {
        *cp = '\0';
    }
    sp = hashLookup(actionTable, actionName);
    if (sp == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Action %s is not defined", actionName);
    } else {
        fn = (WebsAction) sp->content.value.symbol;
        assert(fn);
        if (fn) {
#if ME_GOAHEAD_LEGACY
            (*((WebsProc) fn))((void*) wp, actionName, wp->query);
#else
            (*fn)((void*) wp);
#endif
        }
    }
    return 1;
}


/*
    Define a function in the "action" map space
 */
PUBLIC int websDefineAction(cchar *name, void *fn)
{
    assert(name && *name);
    assert(fn);

    if (fn == NULL) {
        return -1;
    }
    hashEnter(actionTable, (char*) name, valueSymbol(fn), 0);
    return 0;
}


static void closeAction(void)
{
    if (actionTable != -1) {
        hashFree(actionTable);
        actionTable = -1;
    }
}


PUBLIC void websActionOpen(void)
{
    actionTable = hashCreate(WEBS_HASH_INIT);
    websDefineHandler("action", 0, actionHandler, closeAction, 0);
}


#if ME_GOAHEAD_LEGACY
/*
    Don't use these routes. Use websWriteHeaders, websEndHeaders instead.

    Write a webs header. This is a convenience routine to write a common header for an action back to the browser.
 */
PUBLIC void websHeader(Webs *wp)
{
    assert(websValid(wp));

    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    websWrite(wp, "<html>\n");
}


PUBLIC void websFooter(Webs *wp)
{
    assert(websValid(wp));
    websWrite(wp, "</html>\n");
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
