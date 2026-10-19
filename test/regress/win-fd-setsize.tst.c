/*
    Define FD_SETSIZE before Winsock headers so Windows retains the POSIX connection capacity.
 */
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    tinfo("win-fd-setsize: FD_SETSIZE %d, connection limit %d, per-peer limit %d\n", (int) FD_SETSIZE,
          (int) ME_GOAHEAD_LIMIT_CONNECTIONS, (int) ME_GOAHEAD_LIMIT_CONNECTIONS_PER_IP);
    ttrue(FD_SETSIZE >= 256, "the select set must hold more than Winsock's default 64 sockets, got %d",
          (int) FD_SETSIZE);
    ttrue(ME_GOAHEAD_LIMIT_CONNECTIONS >= 128, "the connection limit must not be derived from a 64-socket set, got %d",
          (int) ME_GOAHEAD_LIMIT_CONNECTIONS);
    ttrue(ME_GOAHEAD_LIMIT_CONNECTIONS > ME_GOAHEAD_LIMIT_CONNECTIONS_PER_IP,
          "the total limit %d must exceed the per-peer limit %d, or one address can take every connection",
          (int) ME_GOAHEAD_LIMIT_CONNECTIONS, (int) ME_GOAHEAD_LIMIT_CONNECTIONS_PER_IP);
    return 0;
}
