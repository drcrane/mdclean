#ifndef MOCKFCGIAPP_H
#define MOCKFCGIAPP_H

typedef struct FCGX_Stream {
    unsigned char *rdNext;    /* reader: first valid byte
                               * writer: equals stop */
    unsigned char *wrNext;    /* writer: first free byte
                               * reader: equals stop */
    unsigned char *stop;      /* reader: last valid byte + 1
                               * writer: last free byte + 1 */
    unsigned char *stopUnget; /* reader: first byte of current buffer
                               * fragment, for ungetc
                               * writer: undefined */
    int isReader;
    int isClosed;
    int wasFCloseCalled;
    int FCGI_errno;                /* error status */
    void (*fillBuffProc) (struct FCGX_Stream *stream);
    void (*emptyBuffProc) (struct FCGX_Stream *stream, int doClose);
    void *data;
} FCGX_Stream;

typedef struct FCGX_Request {
    int requestId;            /* valid if isBeginProcessed */
    int role;
    FCGX_Stream *in;
    FCGX_Stream *out;
    FCGX_Stream *err;
    char **envp;
} FCGX_Request;

typedef char ** FCGX_ParamArray;

char * FCGX_GetParam(const char * name, FCGX_ParamArray envp);

int FCGX_GetStr(char * str, int n, FCGX_Stream * stream);

int FCGX_PutStr(const char * str, int n, FCGX_Stream * stream);

#endif /* MOCKFCGIAPP_H */
