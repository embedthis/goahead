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
    Return a copy of the action name, the second segment of the path. The caller must free it.
 */
static char *getActionName(Webs *wp)
{
    char *name, *end;

    if ((name = strchr(&wp->path[1], '/')) == NULL) {
        return NULL;
    }
    name++;
    if ((end = strchr(name, '/')) == NULL) {
        end = name + slen(name);
    }
    return snclone(name, end - name);
}


/*
    Decline actions outside this route so later routes enforce their own policies.
 */
static bool actionMatch(Webs *wp)
{
    char *name;
    bool served;

    if ((name = getActionName(wp)) == NULL) {
        return 1;
    }
    served = websRouteServesAction(wp->route, name);
    wfree(name);
    return served;
}


/*
    Process an action request. Returns 1 always to indicate it handled the URL
    Return true to indicate the request was handled, even for errors.
 */
static bool actionHandler(Webs *wp)
{
    WebsKey    *sp;
    char       *actionName;
    WebsAction fn;

#if ME_GOAHEAD_LEGACY
    WebsProc proc;
#endif

    assert(websValid(wp));
    assert(actionTable >= 0);

    if ((actionName = getActionName(wp)) == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Missing action name");
        return 1;
    }
    /*
        Default state-changing actions to POST.
     */
    if (wp->route && wp->route->methods < 0 && !smatch(wp->method, "POST")) {
        websSetStatus(wp, HTTP_CODE_BAD_METHOD);
        websWriteHeaders(wp, 0, 0);
        websWriteHeader(wp, "Allow", "POST");
        websWriteEndHeaders(wp);
        websDone(wp);
        wfree(actionName);
        return 1;
    }
    sp = hashLookup(actionTable, actionName);
    if (sp == NULL) {
        websError(wp, HTTP_CODE_NOT_FOUND, "Action %s is not defined", actionName);
    } else {
        /*
            Routes choose methods; application handlers must validate CSRF tokens.
            The key's arg records whether the action was defined with the legacy signature.
         */
#if ME_GOAHEAD_LEGACY
        if (sp->arg) {
            proc = (WebsProc) sp->content.value.symbol;
            (*proc)(wp, actionName, wp->query);
            return 1;
        }
#endif
        fn = (WebsAction) sp->content.value.symbol;
        (*fn)(wp);
    }
    wfree(actionName);
    return 1;
}


static int defineAction(cchar *name, void *fn, int legacy)
{
    assert(name && *name);
    assert(fn);

    if (fn == NULL) {
        return -1;
    }
    hashEnter(actionTable, name, valueSymbol(fn), legacy);
    return 0;
}


/*
    Define a function in the "action" map space. Legacy builds call it with the WebsProc signature.
 */
PUBLIC int websDefineAction(cchar *name, void *fn)
{
    return defineAction(name, fn, ME_GOAHEAD_LEGACY);
}


PUBLIC int websDefineNativeAction(cchar *name, WebsAction fn)
{
    return defineAction(name, fn, 0);
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
    websDefineHandler("action", actionMatch, actionHandler, closeAction, 0);
}


#if ME_GOAHEAD_LEGACY
/*
    Don't use these routines. Use websWriteHeaders, websWriteEndHeaders instead.

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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
