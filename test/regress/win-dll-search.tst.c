/* websHardenProcess removes the working directory from the Windows DLL search path */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_WIN_LIKE && !WINCE
#define PROBE "zdllprobe.dll"

static char original[MAX_PATH], probeDir[MAX_PATH], probePath[MAX_PATH], source[MAX_PATH];
#endif

int main(int argc, char **argv)
{
#if ME_WIN_LIKE && !WINCE
    HMODULE lib;
    DWORD   n;

    n = GetCurrentDirectoryA(sizeof(original), original);
    ttrue(n > 0 && n < sizeof(original), "save the working directory");

    /* Use a system library under another name so no foreign code is needed */
    n = GetSystemDirectoryA(source, sizeof(source));
    ttrue(n > 0 && n + 12 < sizeof(source), "locate the system directory");
    strcat(source, "\\version.dll");

    n = GetTempPathA(sizeof(probeDir), probeDir);
    ttrue(n > 0 && n + 40 < sizeof(probeDir), "locate the temp directory");
    sprintf(probeDir + n, "goahead-dllprobe-%lu", (unsigned long) GetCurrentProcessId());
    CreateDirectoryA(probeDir, NULL);
    sprintf(probePath, "%s\\%s", probeDir, PROBE);
    ttrue(CopyFileA(source, probePath, FALSE), "copy the probe library into the probe directory");
    ttrue(SetCurrentDirectoryA(probeDir), "enter the probe directory");

    /* The legacy search order includes the working directory */
    lib = LoadLibraryA(PROBE);
    if (lib == NULL) {
        SetCurrentDirectoryA(original);
        DeleteFileA(probePath);
        RemoveDirectoryA(probeDir);
        tskip("this host already excludes the working directory from the DLL search path");
        return 0;
    }
    FreeLibrary(lib);

    websHardenProcess();

    lib = LoadLibraryA(PROBE);
    ttrue(lib == NULL, "a library in the working directory must not load after hardening");
    if (lib) {
        FreeLibrary(lib);
    } else {
        ttrue(GetLastError() == ERROR_MOD_NOT_FOUND, "the library must not be found, error %lu",
              (unsigned long) GetLastError());
    }
    SetCurrentDirectoryA(original);
    DeleteFileA(probePath);
    RemoveDirectoryA(probeDir);
#else
    tskip("Windows only");
#endif
    return 0;
}
