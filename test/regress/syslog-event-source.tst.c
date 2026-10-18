/* The first Windows syslog message registers the event source under the app name */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_WIN_LIKE
#define EVENT_LOG "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application"

void syslog(int priority, char *format, ...);

int main(int argc, char **argv)
{
    HKEY  hkey;
    DWORD type, size;
    LONG  rc;
    char  value[256];

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, EVENT_LOG, 0, KEY_ALL_ACCESS, &hkey) != ERROR_SUCCESS) {
        tskip("Registering an event source needs administrator rights");
        return 0;
    }
    RegDeleteKey(hkey, ME_NAME);
    RegCloseKey(hkey);

    syslog(0, "%s", "syslog-event-source test message");

    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, EVENT_LOG "\\" ME_NAME, 0, KEY_READ, &hkey);
    teqi(rc, ERROR_SUCCESS, "the event source is registered under the application name");
    if (rc == ERROR_SUCCESS) {
        size = sizeof(value);
        rc = RegQueryValueEx(hkey, "EventMessageFile", NULL, &type, (uchar*) value, &size);
        teqi(rc, ERROR_SUCCESS, "with its message file");
        RegCloseKey(hkey);
    }
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("The event-log syslog applies only to Windows builds");
    return 0;
}
#endif
