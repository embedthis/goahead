/*
    mbedtls-pipe-fixture.h - An in-memory transport for MbedTLS fixtures
 */

#ifndef _h_MBEDTLS_PIPE_FIXTURE
#define _h_MBEDTLS_PIPE_FIXTURE 1

typedef struct Queue {
    uchar  data[64 * 1024];
    size_t len;
} Queue;

typedef struct Pipe {
    Queue *in;
    Queue *out;
} Pipe;

static Queue  toServer, toClient;
static size_t sendLimit = sizeof(((Queue*) 0)->data);

static int pipeSend(void *ctx, cuchar *buf, size_t len)
{
    Queue *q = ((Pipe*) ctx)->out;

    if (len > sendLimit - q->len) {
        len = sendLimit - q->len;
    }
    if (len == 0) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    memcpy(q->data + q->len, buf, len);
    q->len += len;
    return (int) len;
}

static int pipeRecv(void *ctx, uchar *buf, size_t len)
{
    Queue *q = ((Pipe*) ctx)->in;

    if (q->len == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    if (len > q->len) {
        len = q->len;
    }
    memcpy(buf, q->data, len);
    memmove(q->data, q->data + len, q->len - len);
    q->len -= len;
    return (int) len;
}

#endif /* _h_MBEDTLS_PIPE_FIXTURE */
