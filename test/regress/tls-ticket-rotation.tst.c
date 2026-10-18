/*
    tls-ticket-rotation.tst.c - Session ticket keys are replaced, and old ones forgotten
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
#if ME_COM_OPENSSL
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/openssl.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

#if ME_COM_OPENSSL && OPENSSL_VERSION_NUMBER >= 0x30000000L
#include "tls-cert-fixture.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static SSL_SESSION *freshSession(SSL_CTX *clientCtx)
{
    SSL_SESSION *session;

    session = 0;
    handshake(clientCtx, NULL, &session);
    return session;
}


int main(int argc, char **argv)
{
    SSL_CTX     *clientCtx;
    SSL_SESSION *session;
    uchar       name[16];

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    websSetSslCertFile("../pki/self.crt");
    websSetSslKeyFile("../pki/self.key");
    teq(sslOpen(), 0, "open TLS");
    ttrue(ticketKeys[0].created != 0, "sslOpen makes a ticket key");
    clientCtx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(clientCtx, SSL_VERIFY_NONE, NULL);

    tinfo("\n--- Suite A: current and previous keys ---\n");
    session = freshSession(clientCtx);
    ttrue(session && SSL_SESSION_is_resumable(session), "a full handshake issues a ticket");
    teq(handshake(clientCtx, session, NULL), 1, "a ticket under the current key resumes");

    memcpy(name, ticketKeys[0].name, sizeof(name));
    rotateTicketKeys();
    ttrue(memcmp(ticketKeys[1].name, name, sizeof(name)) == 0, "rotation keeps the replaced key as the previous");
    teq(handshake(clientCtx, session, NULL), 1, "a ticket under the previous key still resumes");

    tinfo("\n--- Suite B: a key two rotations old ---\n");
    rotateTicketKeys();
    teq(handshake(clientCtx, session, NULL), 0, "a ticket under a key two rotations old must not resume");
    SSL_SESSION_free(session);

    tinfo("\n--- Suite C: key age ---\n");
    session = freshSession(clientCtx);
    teq(handshake(clientCtx, session, NULL), 1, "a fresh ticket resumes");
    ticketKeys[0].created -= 2 * ME_GOAHEAD_SSL_TIMEOUT;
    teq(handshake(clientCtx, session, NULL), 0, "a key past twice the timeout must not be accepted");
    SSL_SESSION_free(session);

    memcpy(name, ticketKeys[0].name, sizeof(name));
    ticketKeys[0].created -= ME_GOAHEAD_SSL_TIMEOUT;
    session = freshSession(clientCtx);
    ttrue(memcmp(ticketKeys[0].name, name, sizeof(name)) != 0,
          "a key that has issued tickets for the timeout is replaced at the next ticket");
    teq(handshake(clientCtx, session, NULL), 1, "the ticket from the replacement key resumes");
    SSL_SESSION_free(session);

    SSL_CTX_free(clientCtx);
    sslClose();
    ttrue(ticketKeys[0].created == 0 && ticketKeys[1].created == 0, "sslClose erases the ticket keys");
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("OpenSSL 3 backend only");
    return 0;
}
#endif
