/*******************************************************************************
*                                                                              *
*                           INTERNET ACCESS LIBRARY                            *
*                                                                              *
*                       Copyright (C) 2006 Scott A. Franco                     *
*                                                                              *
*                              5/06 S. A. Franco                               *
*                                                                              *
* Implements access to Internet functions, via tcp/ip. tcp/ip is implemented   *
* via the "file" paradigm. An address and port is used to create a file, then  *
* normal C read and write statements are used to access it.                    *
*                                                                              *
* Windows sockets are not CRT file descriptors, so unlike Linux the socket     *
* cannot itself back the FILE. Instead each connection parks a FILE on the     *
* "nul" device and records the socket in a table indexed by the CRT fd; the    *
* system call interdictions (read/write/close) then divert I/O on that fd to   *
* the socket. Security (TLS/DTLS) is provided by OpenSSL talking directly to   *
* the socket, so no shadow fd is needed as on Linux.                           *
*                                                                              *
* Note that I often use SSL to mean "secure socket attached to standard FILE   *
* structure, which nowadays means TLS. I also use DTLS to refer to message     *
* based sockets using DTLS on UDP.                                             *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
*                                                                              *
* Portions of the code Copyright (C) 2009 - 2012 Robin Seggelmann,             *
* seggelmann@fh-muenster.de, Michael Tuexen, tuexen@fh-muenster.de             *
* 2019 Felix Weinrank, weinrank@fh-muenster.de, The OpenSSL Project Authors.   *
*                                                                              *
* All rights reserved.                                                         *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions           *
* are met:                                                                     *
*                                                                              *
* 1. Redistributions of source code must retain the above copyright            *
*    notice, this list of conditions and the following disclaimer.             *
* 2. Redistributions in binary form must reproduce the above copyright         *
*    notice, this list of conditions and the following disclaimer in the       *
*    documentation and/or other materials provided with the distribution.      *
* 3. Neither the name of the project nor the names of its contributors         *
*    may be used to endorse or promote products derived from this software     *
*    without specific prior written permission.                                *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND      *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE        *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   *
* ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE     *
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL   *
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS      *
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)        *
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT   *
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    *
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       *
* SUCH DAMAGE.                                                                 *
*                                                                              *
* ============================================================================ *
*                                                                              *
* For the portions of the code covered by The OpenSSL License:                 *
*                                                                              *
* Licensed under the Apache License 2.0 (the "License").  You may not use      *
* this file except in compliance with the License.  You can obtain a copy      *
* in the file LICENSE in the source distribution or at                         *
* https://www.openssl.org/source/license.html                                  *
*                                                                              *
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00

/* Windows/mingw definitions */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
/* winsock2 must precede windows.h */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/* OpenSSL */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/x509v3.h>

/* OpenSSL 3.0 renamed SSL_get_peer_certificate to SSL_get1_peer_certificate.
   The old name survives only as a deprecated alias, which mingw's OpenSSL 3.x
   build compiles out, so map it to the current name. Both return a reference
   the caller must X509_free. */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#ifndef SSL_get_peer_certificate
#define SSL_get_peer_certificate SSL_get1_peer_certificate
#endif
#endif

/* Petit-Ami definitions */
#include <localdefs.h>
#include <network.h>

/*
 * Debug print system
 *
 * Example use:
 *
 * dbg_printf(dlinfo, "There was an error: string: %s\n", bark);
 *
 * mydir/test.c:myfunc():12: There was an error: somestring
 *
 */

static enum { /* debug levels */

    dlinfo, /* informational */
    dlwarn, /* warnings */
    dlfail, /* failure/critical */
    dlnone  /* no messages */

} dbglvl = dlinfo;

#define dbg_printf(lvl, fmt, ...) \
        do { if (lvl >= dbglvl) fprintf(stderr, "%s:%s():%d: " fmt, __FILE__, \
                                __func__, __LINE__, ##__VA_ARGS__); } while (0)

#define MAXFIL 1000 /* maximum number of open files */
#define COOKIE_SECRET_LENGTH 16 /* length of secret cookie */
#define CVBUFSIZ 4096 /* certificate value buffer size */
#define CONRETRY 50  /* connect retries on refused connections */
#define CONDELAY 100 /* delay between connect retries, in milliseconds
                        (0.1 second, under human perception; with CONRETRY
                        gives a 5 second budget before the error stands) */
#define DTLSPAY  16384  /* most payload one DTLS record carries: a datagram
                           carries at most one record, whatever the route
                           itself could hold */
#define DTLSOVR  64     /* record header and cipher expansion riding inside
                           the datagram; a generous bound across cipher
                           suites */

/* socket structures */
typedef union {

    struct sockaddr_storage ss;
    struct sockaddr_in6 s6;
    struct sockaddr_in s4;

} socket_struct;

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int     (*popen_t)(const char*, int, int);
typedef int     (*pclose_t)(int);
typedef off_t   (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/* File tracking.
   Windows sockets cannot back a CRT fd, so each network connection parks a
   FILE (or bare fd for message sockets) on the "nul" device and the actual
   socket lives here, indexed by that fd. The interdicted read/write/close
   calls then divert to the socket. SSL and DTLS state also live here. */
typedef struct filrec {

    int           net;    /* it's a network file */
    int           sec;    /* it's a secure sockets (TLS) file */
    int           msg;    /* is a message socket (udp/dtls) */
    int           sudp;   /* is a secure udp (DTLS) */
    int           v6addr; /* is an IPv6 address */
    SOCKET        sock;   /* handle to network socket */
    SOCKET        sock2;  /* second socket (DTLS server connected side) */
    socket_struct saddr;  /* socket address structure */
    SSL*          ssl;    /* SSL data */
    X509*         cert;   /* peer certificate */
    BIO*          bio;    /* bio for DTLS */

} filrec, *filptr;

/* error codes */
typedef enum {

    ewskini,  /* cannot initialize winsock */
    einvhan,  /* invalid file handle */
    enetopn,  /* cannot open network file */
    enetpos,  /* cannot position network file */
    enetloc,  /* cannot find location network file */
    enetlen,  /* cannot find length network file */
    esckeof,  /* End encountered on socket */
    efinuse,  /* file already in use */
    enetwrt,  /* Attempt to write to input side of network pair */
    enetadr,  /* cannot determine address of server */
    einissl,  /* Cannot initialize OpenSSL library */
    esslnew,  /* Cannot create SSL */
    esslctx,  /* Cannot create SSL context */
    esslfid,  /* Cannot connect SSL to file id */
    esslses,  /* Cannot create SSL session */
    esslcer,  /* Cannot get SSL certificate */
    enodupf,  /* Cannot create duplicate fid */
    enoallc,  /* Cannot allocate file entry */
    enolcert, /* Cannot load certificate */
    enolpkey, /* Cannot load private key */
    enotmsg,  /* not a message file id */
    eismsg,   /* is a message file id */
    enotsec,  /* Not a secured file */
    enocert,  /* Cannot retrieve certificate */
    enobio,   /* Cannot create BIO in OpenSSL */
    ewrbio,   /* Cannot write to BIO in OpenSSL */
    erdbio,   /* Cannot read from BIO in OpenSSL */
    ecerttl,  /* PEM format certificate too large for buffer */
    einvctn,  /* invalid certificate number */
    enoserl,  /* Cannot get serial number */
    enosiga,  /* Could not get signature algorithm */
    enomem,   /* Out of memory */
    ebufovf,  /* Buffer overflow */
    ecertpar, /* Error parsing certificate data */
    enoipv4,  /* Cannot find IPV4 address */
    enotimp,  /* function not implemented */
    esystem   /* System consistency check */

} errcod;

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overridden by this module.
 *
 */
static pread_t   ofpread;
static pwrite_t  ofpwrite;
static popen_t   ofpopen;
static pclose_t  ofpclose;
static plseek_t  ofplseek;

static filptr opnfil[MAXFIL];      /* open files table */
static CRITICAL_SECTION netlck;    /* lock for files table and lazy contexts */
static ami_certptr frecert;        /* free certificate name/value entries list */

/* The double fault flag is set when exiting, so if we exit again, it
  is checked, then forces an immediate exit. This keeps faults from
  looping. */
static int dblflt; /* double fault flag */

/*
 * openSSL variables
 */

/* contexts */
static SSL_CTX* client_tls_ctx;
static SSL_CTX* client_dtls_ctx;
static SSL_CTX* server_tls_ctx;
static SSL_CTX* server_dtls_ctx;

/* Each SSL context loads certificate and key .pem files. Those files need not
   exist for a program that performs no secure networking, so the contexts are
   created lazily, on the first secure use of each, rather than in the startup
   constructor. Otherwise every program linking this library would have to carry
   the .pem files (and would abort at startup without them). The netlck guards
   make the lazy creation safe under the multithreaded server. */

/* server secret cookie */
static unsigned char cookie_secret[COOKIE_SECRET_LENGTH];
/* cookie has been initialized */
static int cookie_initialized;

/*******************************************************************************

Process network library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

/* the installed error handler; a handler that longjmps takes the
   error, one that returns lets the abort proceed */
static ami_neterrhan_t neterrhan;

void ami_neterror(ami_neterrhan_t handler)

{

    neterrhan = handler;

}

static void netwrterr(const char* s)

{

    fprintf(stderr, "\nError: Network: %s\n", s);

    if (neterrhan) neterrhan(s);
    /* under diagnosis, die where it happened: the crash dump reports the
       stack of this thread */
    if (getenv("AMI_ERRABORT")) abort();
    exit(1);

}

/*******************************************************************************

Print error

Prints the given error in ASCII text, then aborts the program.
This needs to go to a dialog instead of the system error trap.

*******************************************************************************/

static void error(errcod e)

{

    switch (e) { /* error */

        case ewskini:  netwrterr("Cannot initialize winsock"); break;
        case einvhan:  netwrterr("Invalid file number"); break;
        case enetopn:  netwrterr("Cannot reset or rewrite network file");
                       break;
        case enetpos:  netwrterr("Cannot position network file"); break;
        case enetloc:  netwrterr("Cannot find location network file"); break;
        case enetlen:  netwrterr("Cannot find length network file"); break;
        case esckeof:  netwrterr("End encountered on socket"); break;
        case efinuse:  netwrterr("File already in use"); break;
        case enetwrt:
            netwrterr("Attempt to write to input side of network pair"); break;
        case enetadr:  netwrterr("Cannot determine address of server"); break;
        case einissl:  netwrterr("Cannot initialize OpenSSL library"); break;
        case esslnew:  netwrterr("Cannot create SSL"); break;
        case esslctx:  netwrterr("Cannot create SSL context"); break;
        case esslfid:  netwrterr("Cannot connect SSL to file id"); break;
        case esslses:  netwrterr("Cannot create SSL session"); break;
        case esslcer:  netwrterr("Cannot get SSL certificate"); break;
        case enodupf:  netwrterr("Cannot create duplicate fid"); break;
        case enoallc:  netwrterr("Cannot allocate file entry"); break;
        case enolcert: netwrterr("Cannot load certificate"); break;
        case enolpkey: netwrterr("Cannot load private key"); break;
        case enotmsg:  netwrterr("Not a message file id"); break;
        case eismsg:   netwrterr("Is a message file id"); break;
        case enotsec:  netwrterr("Not a secured file"); break;
        case enocert:  netwrterr("Cannot retrieve certificate"); break;
        case enobio:   netwrterr("Cannot create BIO in OpenSSL"); break;
        case ewrbio:   netwrterr("Cannot write to BIO in OpenSSL"); break;
        case erdbio:   netwrterr("Cannot read from BIO in OpenSSL"); break;
        case ecerttl:  netwrterr("PEM format certificate too large for buffer");
                       break;
        case einvctn:  netwrterr("Invalid certificate number"); break;
        case enoserl:  netwrterr("Cannot get serial number"); break;
        case enosiga:  netwrterr("Could not get signature algorithm"); break;
        case enomem:   netwrterr("Out of memory"); break;
        case ebufovf:  netwrterr("Buffer overflow"); break;
        case ecertpar: netwrterr("Error parsing certificate data"); break;
        case enoipv4:  netwrterr("Cannot find IPV4 address"); break;
        case enotimp:  netwrterr("Function not implemented"); break;
        case esystem:
            netwrterr("System consistency check, please contact vendor"); break;

    }

}

/*******************************************************************************

Print dotted address

Prints an address in the form 1.2.3.4. A diagnostic.

*******************************************************************************/

static void prtadr(ami_ulong addr)

{

    fprintf(stderr, "%d.%d.%d.%d", (int)(addr >> 24 & 0xff),
            (int)(addr >> 16 & 0xff), (int)(addr >> 8 & 0xff),
            (int)(addr & 0xff));

}

/*******************************************************************************

Handle Winsock error

Only called if the last error variable is set. The text string for the error
is output, and then the program halted.

*******************************************************************************/

static void wskerr(void)
{

    LPVOID lpMsgBuf;

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL, WSAGetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf,
                  0, NULL);
    fprintf(stderr, "\n*** Windows error: %s\n", (char*)lpMsgBuf);

    if (neterrhan) neterrhan("Windows socket error");
    exit(1);

}

/*******************************************************************************

Handle SSL error queue

Dumps the SSL error queue.

*******************************************************************************/

static int sslerrcb(const char *str, size_t len, void *u)

{

    (void)u;
    fprintf(stderr, "%.*s", (int)len, str);
    return 1;

}

static void sslerrorqueue(void)

{

    ERR_print_errors_cb(sslerrcb, NULL);
    if (neterrhan) neterrhan("SSL error");
    exit(1);

}

/*******************************************************************************

Handle SSL error

Handles an SSL layer error by looking up the error code and printing the
appropriate error message.

*******************************************************************************/

static void sslerror(SSL* ssl, int r)

{

    fprintf(stderr, "\nSSL Error: ");
    switch (SSL_get_error(ssl, r)) {

        case SSL_ERROR_NONE:
            fprintf(stderr, "The TLS/SSL I/O operation completed\n");
            break;
        case SSL_ERROR_ZERO_RETURN:
            fprintf(stderr, "The TLS/SSL connection has been closed\n");
            break;
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_ACCEPT:
            fprintf(stderr, "The operation did not complete\n");
            break;
        case SSL_ERROR_WANT_X509_LOOKUP:
            fprintf(stderr, "The operation did not complete because an application "
                      "callback set by SSL_CTX_set_client_cert_cb() has asked "
                      "to be called again\n");
            break;
        case SSL_ERROR_SYSCALL:
            fprintf(stderr, "System I/O error\n");
            break;
        case SSL_ERROR_SSL:
            fprintf(stderr, "A failure in the SSL library occurred\n");
            sslerrorqueue();
            break;
        default:
            fprintf(stderr, "Unknown error code\n");
            break;

    }

    if (neterrhan) neterrhan("SSL error");
    exit(1);

}

/*******************************************************************************

Get file entry

Gets a file entry by allocation. Clears the fields in the file structure.

*******************************************************************************/

static filptr getfil(void)

{

    filptr fp;

    /* get entry */
    fp = malloc(sizeof(filrec));
    if (!fp) error(enomem); /* didn't work */
    fp->net = FALSE;    /* set not network file */
    fp->sec = FALSE;    /* set ordinary socket */
    fp->msg = FALSE;    /* set not message port */
    fp->sudp = FALSE;   /* set not secure udp */
    fp->v6addr = FALSE; /* set v4 address */
    fp->sock = INVALID_SOCKET;  /* clear socket */
    fp->sock2 = INVALID_SOCKET; /* clear second socket */
    fp->ssl = NULL;     /* clear SSL data */
    fp->cert = NULL;    /* clear certificate data */
    fp->bio = NULL;     /* clear BIO data */

    return (fp);

}

/*******************************************************************************

Get new file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated. Then the file entry is initialized.

*******************************************************************************/

static void newfil(int fn)

{

    filptr fp;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    EnterCriticalSection(&netlck); /* take the table lock */
    /* See if the file entry is undefined, then get a file entry if not */
    if (!opnfil[fn]) opnfil[fn] = getfil();
    else {

        fp = opnfil[fn]; /* index that */
        fp->net = FALSE;    /* set unoccupied */
        fp->sec = FALSE;    /* set ordinary socket */
        fp->msg = FALSE;    /* set not message port */
        fp->sudp = FALSE;   /* set not secure udp */
        fp->v6addr = FALSE; /* set v4 address */
        fp->sock = INVALID_SOCKET;  /* clear socket */
        fp->sock2 = INVALID_SOCKET; /* clear second socket */
        fp->ssl = NULL;     /* clear SSL data */
        fp->cert = NULL;    /* clear certificate data */
        fp->bio = NULL;     /* clear BIO data */

    }
    LeaveCriticalSection(&netlck); /* release the table lock */

}

/*******************************************************************************

Make file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated. Unlike newfil, an existing entry is left as it stands.

*******************************************************************************/

static void makfil(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    EnterCriticalSection(&netlck); /* take the table lock */
    if (!opnfil[fn]) opnfil[fn] = getfil();
    LeaveCriticalSection(&netlck); /* release the table lock */

}

/*******************************************************************************

Get certificate name/value entry

Recycles or allocates a name/value entry.

*******************************************************************************/

static ami_certptr getcert(void)

{

    ami_certptr cp;

    if (frecert) { /* there are free cert entries */

        cp = frecert; /* get top entry */
        frecert = cp->next; /* remove from free list */

    } else cp = malloc(sizeof(ami_certfield));
    if (!cp) error(enomem); /* cannot allocate entry */
    /* clear fields */
    cp->name = NULL;
    cp->data = NULL;
    cp->critical = FALSE;
    cp->fork = NULL;
    cp->next = NULL;

    return (cp); /* return entry */

}

/*******************************************************************************

Put certificate name/value entry

Free a certificate name/value entry. Recycling cert data entries is optional,
but can reduce memory fragmentation. The strings attached to these entries are
always recycled, and thus does help with fragmention for those. There are future
means to do that, such as allocating from blocks of characters.

If a tree structured entry is passed, then the entire tree is freed.

*******************************************************************************/

static void putcert(ami_certptr cp)

{

    ami_certptr p;

    /* release strings space */
    free(cp->name);
    free(cp->data);
    /* free sublist */
    while (cp->fork) { /* traverse the list */

        p = cp->fork; /* top entry from list */
        cp->fork = cp->fork->next;
        putcert(p); /* free entry */

    }
    free(cp); /* free the entry */

}

/*******************************************************************************

Generate DTLS cookie

Generates a cookie for DTLS communications. A cookie is generated for the given
ssl socket, and placed into the given buffer. The length of the cookie is also
set.

This code comes from:

https://github.com/nplab/DTLS-Examples

*******************************************************************************/

static int generate_cookie(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)

{

    unsigned char *buffer, result[EVP_MAX_MD_SIZE];
    unsigned int length = 0, resultlength;
    socket_struct peer;

    /* Initialize a random secret */
    if (!cookie_initialized)
        {
        if (!RAND_bytes(cookie_secret, COOKIE_SECRET_LENGTH))
            {
            printf("error setting random cookie secret\n");
            return 0;
            }
        cookie_initialized = TRUE;
        }

    /* Read peer information */
    (void) BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

    /* Create buffer with peer's address and port */
    length = 0;
    switch (peer.ss.ss_family) {
        case AF_INET:
            length += sizeof(struct in_addr);
            break;
        case AF_INET6:
            length += sizeof(struct in6_addr);
            break;
        default:
            OPENSSL_assert(0);
            break;
    }
    length += sizeof(unsigned short);
    buffer = (unsigned char*) OPENSSL_malloc(length);

    if (buffer == NULL)
        {
        printf("out of memory\n");
        return 0;
        }

    switch (peer.ss.ss_family) {
        case AF_INET:
            memcpy(buffer,
                   &peer.s4.sin_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(peer.s4.sin_port),
                   &peer.s4.sin_addr,
                   sizeof(struct in_addr));
            break;
        case AF_INET6:
            memcpy(buffer,
                   &peer.s6.sin6_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(unsigned short),
                   &peer.s6.sin6_addr,
                   sizeof(struct in6_addr));
            break;
        default:
            OPENSSL_assert(0);
            break;
    }

    /* Calculate HMAC of buffer using the secret */
    HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
         (const unsigned char*) buffer, length, result, &resultlength);
    OPENSSL_free(buffer);

    memcpy(cookie, result, resultlength);
    *cookie_len = resultlength;

    return 1;

}

/*******************************************************************************

Verify DTLS cookie

Verifies a cookie for DTLS communications. A cookie is checked for the given
ssl socket. The cookie string is expected in a buffer, and the length of the
string is also given.

This code comes from:

https://github.com/nplab/DTLS-Examples

*******************************************************************************/

static int verify_cookie(SSL *ssl, const unsigned char *cookie, unsigned int cookie_len)

{

    unsigned char *buffer, result[EVP_MAX_MD_SIZE];
    unsigned int length = 0, resultlength;
    socket_struct peer;

    /* If secret isn't initialized yet, the cookie can't be valid */
    if (!cookie_initialized)
        return 0;

    /* Read peer information */
    (void) BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

    /* Create buffer with peer's address and port */
    length = 0;
    switch (peer.ss.ss_family) {
        case AF_INET:
            length += sizeof(struct in_addr);
            break;
        case AF_INET6:
            length += sizeof(struct in6_addr);
            break;
        default:
            OPENSSL_assert(0);
            break;
    }
    length += sizeof(unsigned short);
    buffer = (unsigned char*) OPENSSL_malloc(length);

    if (buffer == NULL)
        {
        printf("out of memory\n");
        return 0;
        }

    switch (peer.ss.ss_family) {
        case AF_INET:
            memcpy(buffer,
                   &peer.s4.sin_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(unsigned short),
                   &peer.s4.sin_addr,
                   sizeof(struct in_addr));
            break;
        case AF_INET6:
            memcpy(buffer,
                   &peer.s6.sin6_port,
                   sizeof(unsigned short));
            memcpy(buffer + sizeof(unsigned short),
                   &peer.s6.sin6_addr,
                   sizeof(struct in6_addr));
            break;
        default:
            OPENSSL_assert(0);
            break;
    }

    /* Calculate HMAC of buffer using the secret */
    HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
         (const unsigned char*) buffer, length, result, &resultlength);
    OPENSSL_free(buffer);

    if (cookie_len == resultlength && memcmp(result, cookie, resultlength) == 0)
        return 1;

    return 0;

}

/*******************************************************************************

Verify certificate

Currently unused. Returns verified always.

*******************************************************************************/

static int dtls_verify_callback (int ok, X509_STORE_CTX *ctx)

{

    return 1;

}

/*******************************************************************************

Initialize SSL context

Note the .pem files are read with our own stdio and passed to OpenSSL as
memory BIOs. The prebuilt OpenSSL library was compiled against the Microsoft
C runtime FILE, which is not compatible with the petit-ami stdio this library
links, so OpenSSL's own file loading calls (SSL_CTX_use_certificate_file and
friends) cannot be used.

*******************************************************************************/

/* read a file into a memory BIO */

static BIO* filebio(const char* fn)

{

    FILE*  fp;
    BIO*   bp;
    char   buff[4096];
    size_t n;

    fp = fopen(fn, "rb");
    if (!fp) {

        fprintf(stderr, "\nError: Network: Cannot open certificate/key file: "
                        "%s\n", fn);
        exit(1);

    }
    bp = BIO_new(BIO_s_mem());
    if (!bp) error(enobio);
    while ((n = fread(buff, 1, sizeof(buff), fp)) > 0)
        BIO_write(bp, buff, (int)n);
    fclose(fp);

    return (bp);

}

static void initctx(
    /* context */ SSL_CTX** ctx,
    /* communications method */ const SSL_METHOD *method,
    /* certificate file */ string cert,
    /* key file */ string key
)

{

    int r;
    BIO* bp;
    X509* certx;
    EVP_PKEY* pkey;

    /* create new SSL context */
    *ctx = SSL_CTX_new(method);
    if (!*ctx) error(esslctx);

    /* Set the key and cert. The first certificate in the file is the entity
       certificate; any following certificates form the chain presented to
       the peer (leaf, then intermediates), matching the chain file form the
       other platforms load */
    bp = filebio(cert);
    certx = PEM_read_bio_X509(bp, NULL, NULL, NULL);
    if (!certx) sslerrorqueue();
    r = SSL_CTX_use_certificate(*ctx, certx);
    X509_free(certx); /* the context holds its own reference */
    if (r <= 0) sslerrorqueue();
    /* add any remaining certificates as the presented chain */
    certx = PEM_read_bio_X509(bp, NULL, NULL, NULL);
    while (certx) {

        /* the context takes ownership of extra chain certificates */
        r = SSL_CTX_add_extra_chain_cert(*ctx, certx);
        if (r <= 0) sslerrorqueue();
        certx = PEM_read_bio_X509(bp, NULL, NULL, NULL);

    }
    ERR_clear_error(); /* clear the end of file "error" from the reads */
    BIO_free(bp);

    bp = filebio(key);
    pkey = PEM_read_bio_PrivateKey(bp, NULL, NULL, NULL);
    if (!pkey) sslerrorqueue();
    BIO_free(bp);
    r = SSL_CTX_use_PrivateKey(*ctx, pkey);
    EVP_PKEY_free(pkey); /* the context holds its own reference */
    if (r <= 0) sslerrorqueue();

    r = SSL_CTX_check_private_key(*ctx);
    if (r != 1) sslerrorqueue();

}

/*******************************************************************************

Lazy SSL context creation

These create each SSL context on first secure use (guarded by the netlck),
loading the relevant certificate/key .pem files only then. A program that does
no secure networking never reaches these, so it never needs the .pem files.

*******************************************************************************/

static void ensure_client_tls(void)

{

    EnterCriticalSection(&netlck);
    if (!client_tls_ctx)
        initctx(&client_tls_ctx, TLS_client_method(), "client_tls_cert.pem",
                                                      "client_tls_key.pem");
    LeaveCriticalSection(&netlck);

}

static void ensure_client_dtls(void)

{

    EnterCriticalSection(&netlck);
    if (!client_dtls_ctx)
        initctx(&client_dtls_ctx, DTLS_client_method(), "client_dtls_cert.pem",
                                                        "client_dtls_key.pem");
    LeaveCriticalSection(&netlck);

}

static void ensure_server_tls(void)

{

    EnterCriticalSection(&netlck);
    if (!server_tls_ctx) {

        initctx(&server_tls_ctx, TLS_server_method(), "server_tls_cert.pem",
                                                      "server_tls_key.pem");
        SSL_CTX_set_ecdh_auto(server_tls_ctx, 1);

    }
    LeaveCriticalSection(&netlck);

}

static void ensure_server_dtls(void)

{

    EnterCriticalSection(&netlck);
    if (!server_dtls_ctx) {

        initctx(&server_dtls_ctx, DTLS_server_method(), "server_dtls_cert.pem",
                                                        "server_dtls_key.pem");
        SSL_CTX_set_ecdh_auto(server_dtls_ctx, 1);

        /* Client has to authenticate */
        SSL_CTX_set_verify(server_dtls_ctx,
                           SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE,
                           dtls_verify_callback);

        SSL_CTX_set_session_cache_mode(server_dtls_ctx, SSL_SESS_CACHE_OFF);
        SSL_CTX_set_cookie_generate_cb(server_dtls_ctx, generate_cookie);
        SSL_CTX_set_cookie_verify_cb(server_dtls_ctx, &verify_cookie);

    }
    LeaveCriticalSection(&netlck);

}

/*******************************************************************************

Get 128 bit v6 address to 64 bit high/low

Gets a high/low 64 bit address value from a 128 bit v6 address. Windows
accesses the v6 address by bytes, which is also endian independent.

*******************************************************************************/

static void get128t64(struct sockaddr_in6* sap, unsigned long long* addrh,
                      unsigned long long* addrl)

{

    unsigned char* bp;
    int i;

    bp = (unsigned char*)&sap->sin6_addr;
    *addrh = 0;
    *addrl = 0;
    for (i = 0; i < 8; i++) *addrh = *addrh << 8 | bp[i];
    for (i = 8; i < 16; i++) *addrl = *addrl << 8 | bp[i];

}

/*******************************************************************************

Get 64 bit high/low v6 address to 128 bit

Gets a 128 bit v6 address from a 64 bit high/low address.

*******************************************************************************/

static void get64t128(unsigned long long addrh, unsigned long long addrl,
                      struct sockaddr_in6* sap)

{

    unsigned char* bp;
    int i;

    bp = (unsigned char*)&sap->sin6_addr;
    for (i = 0; i < 8; i++) bp[i] = addrh >> (7-i)*8 & 0xff;
    for (i = 0; i < 8; i++) bp[i+8] = addrl >> (7-i)*8 & 0xff;

}

/*******************************************************************************

Get server address v4

Retrieves a v4 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnet(const string name, ami_ulong* addr)

{

    struct addrinfo hints;
    struct addrinfo *p, *pl;
    int r;
    int af;

    /* Note that unlike Linux, Windows returns entries with unset (0) socket
       types unless the hints select them */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    af = FALSE; /* set address not found */
    r = getaddrinfo(name, NULL, &hints, &pl);
    if (r) netwrterr(gai_strerror(r));
    p = pl;
    while (p) {

        /* traverse the available addresses */
        if (p->ai_family == AF_INET) {

            /* get the IPv4 address */
            *addr =
                ntohl(((struct sockaddr_in*)(p->ai_addr))->sin_addr.s_addr);
            af = TRUE; /* set an address found */

        }
        p = p->ai_next;

    }
    freeaddrinfo(pl);
    if (!af) error(enoipv4); /* no address found */

}

/*******************************************************************************

Get server address v6

Retrieves a v6 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnetv6(string name, unsigned long long* addrh,
                unsigned long long* addrl)

{

    struct addrinfo hints;
    struct addrinfo *p, *pl;
    int r;
    int af;

    /* Note that unlike Linux, Windows returns entries with unset (0) socket
       types unless the hints select them */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    af = FALSE; /* set address not found */
    r = getaddrinfo(name, NULL, &hints, &pl);
    if (r) netwrterr(gai_strerror(r));
    p = pl;
    while (p) {

        /* traverse the available addresses */
        if (p->ai_family == AF_INET6) {

            /* get the IPv6 address */
            get128t64((struct sockaddr_in6*)(p->ai_addr), addrh, addrl);
            af = TRUE; /* set an address found */

        }
        p = p->ai_next;

    }
    freeaddrinfo(pl);
    if (!af) error(enetadr); /* no address found */

}

/*******************************************************************************

Open network file

Opens a network file with the given address and port. The file access is
broken up into a read and write side, that work independently, can can be
spread among different tasks. This is done because the Pascal file paradigm
does not handle read/write on the same file well.

*******************************************************************************/

/* park a FILE entry on "nul" and attach the given socket to it. Common to
   opennet and waitnet, and performs the TLS negotiation if selected. The
   parked open is serialized under netlck because server and client can run
   on simultaneous threads of this process. */

static FILE* sockfil(SOCKET sock, int secure, int server)

{

    FILE*  fp;  /* file pointer */
    int    fn;  /* file number */
    filptr fep; /* file tracking pointer */
    SSL*   ssl;
    X509*  cert;
    int    r;

    /* open file handle as null device to allocate a CRT fd for the socket */
    EnterCriticalSection(&netlck);
    fp = fopen("nul", "r+");
    LeaveCriticalSection(&netlck);
    if (!fp) error(enetopn);
    fn = fileno(fp); /* get logical file no. */
    newfil(fn); /* get/renew file entry */
    fep = opnfil[fn]; /* index that */
    fep->sock = sock; /* place socket */
    fep->net = TRUE; /* set as network file */

    /* check secure sockets layer, and negotiate if so */
    if (secure) {

        /* The SSL layer talks directly to the socket, not through the CRT
           fd, so unlike Linux no shadow fd is needed to keep the
           interdictions from looping */
        if (server) {

            ensure_server_tls(); /* ensure context */
            ssl = SSL_new(server_tls_ctx); /* create new ssl */
            if (!ssl) error(esslnew);
            r = SSL_set_fd(ssl, (int)sock); /* connect to socket */
            if (!r) error(esslfid);

            /* perform ssl server accept */
            r = SSL_accept(ssl);
            if (r <= 0) sslerrorqueue();

            fep->ssl = ssl;
            fep->sec = TRUE; /* turn TLS encode/decode on for ssl channel */

        } else {

            ensure_client_tls(); /* ensure context */
            ssl = SSL_new(client_tls_ctx); /* create new ssl */
            if (!ssl) error(esslnew);
            r = SSL_set_fd(ssl, (int)sock); /* connect to socket */
            if (!r) error(esslfid);

            /* initiate tls handshake */
            r = SSL_connect(ssl);
            if (r != 1) sslerror(ssl, r);

            /* Get the remote certificate into the X509 structure.
               Right now we don't do anything with this (don't verify it) */
            cert = SSL_get_peer_certificate(ssl);
            if (!cert) error(esslcer);

            fep->ssl = ssl;
            fep->cert = cert;
            fep->sec = TRUE; /* turn TLS encode/decode on for ssl channel */

        }

    }

    return (fp); /* exit with file */

}

FILE* ami_opennet(/* IP address */      ami_ulong addr,
                 /* port */            ami_long port,
                 /* link is secured */ ami_long secure
)

{

    SOCKET sock;
    struct sockaddr_in saddr;
    int    r;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    /* note, parameters specified in big endian */
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(addr);

    /* Open socket as internet, stream, and connect. A refused connection is
       retried on a short backoff: servers are routinely reached just as they
       come up, and the retry heals that transparently. A server that stays
       down still errors after the retry budget */
    for (r = 0; r < CONRETRY; r++) {

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) wskerr(); /* process Winsock error */
        if (connect(sock, (struct sockaddr*)&saddr,
                    sizeof(struct sockaddr_in)) != SOCKET_ERROR) break;
        if (WSAGetLastError() != WSAECONNREFUSED || r == CONRETRY-1)
            wskerr(); /* process Winsock error */
        closesocket(sock); /* a failed connect leaves the socket unusable */
        Sleep(CONDELAY); /* let the server come up */

    }

    /* finish with general routine */
    return (sockfil(sock, secure, FALSE));

}

FILE* ami_opennetv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            ami_long port,
    /* link is secured */ ami_long secure
)

{

    SOCKET sock;
    struct sockaddr_in6 saddr;
    int    r;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);
    saddr.sin6_port = htons(port);

    /* open socket as internet, stream */
    /* connect the socket, with the same refused connection retry as
       ami_opennet */
    for (r = 0; r < CONRETRY; r++) {

        sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) wskerr(); /* process Winsock error */
        if (connect(sock, (struct sockaddr*)&saddr,
                    sizeof(struct sockaddr_in6)) != SOCKET_ERROR) break;
        if (WSAGetLastError() != WSAECONNREFUSED || r == CONRETRY-1)
            wskerr(); /* process Winsock error */
        closesocket(sock); /* a failed connect leaves the socket unusable */
        Sleep(CONDELAY); /* let the server come up */

    }

    /* finish with general routine */
    return (sockfil(sock, secure, FALSE));

}

/*******************************************************************************

Open message file

Opens a message file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either UDP, or
DTLS, with fixed length messages.

*******************************************************************************/

/* allocate a CRT fd on the null device to serve as the logical id of a
   message socket. This keeps message ids in the same space as the stream
   file ids, so the certificate calls can share code */

static int msgfil(SOCKET sock)

{

    int fn;

    EnterCriticalSection(&netlck);
    fn = _open("nul", _O_RDWR);
    LeaveCriticalSection(&netlck);
    if (fn < 0) error(enetopn);
    newfil(fn); /* get/renew file entry */
    opnfil[fn]->sock = sock; /* place socket */
    opnfil[fn]->net = TRUE; /* set network (sockets) file */
    opnfil[fn]->msg = TRUE; /* set message socket */

    return (fn);

}

ami_long ami_openmsg(
    /* ip address */      ami_ulong addr,
    /* port */            ami_long port,
    /* link is secured */ ami_long secure
)

{

    struct sockaddr_in saddr;
    SOCKET sock;
    int fn;
    socket_struct laddr;
    int r;
    struct timeval timeout;
    DWORD tv;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);
    saddr.sin_port = htons(port);

    /* create the socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    fn = msgfil(sock); /* park a fd entry on the socket */

    /* set up server address */
    memset(&opnfil[fn]->saddr.s4, 0, sizeof(struct sockaddr_in));

    /* copy address information from caller */
    memcpy(&opnfil[fn]->saddr.s4, &saddr, sizeof(struct sockaddr_in));

    opnfil[fn]->v6addr = FALSE; /* set v4 address */

    /* set up for DTLS operation if selected */
    if (secure) {

        /* clear local */
        memset((void *) &laddr, 0, sizeof(socket_struct));
        laddr.s4.sin_family = AF_INET;
        laddr.s4.sin_port = htons(0);
        r = bind(sock, (const struct sockaddr *) &laddr,
                 sizeof(struct sockaddr_in));
        if (r == SOCKET_ERROR) wskerr();

        /* create socket struct */
        ensure_client_dtls(); /* ensure context */
        opnfil[fn]->ssl = SSL_new(client_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        /* Create BIO, connect and set to already connected */
        opnfil[fn]->bio = BIO_new_dgram((int)sock, BIO_NOCLOSE);
        r = connect(sock, (struct sockaddr *) &opnfil[fn]->saddr,
                    sizeof(struct sockaddr_in));
        if (r == SOCKET_ERROR) wskerr();

        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0,
                 &opnfil[fn]->saddr.ss);

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);

        r = SSL_connect(opnfil[fn]->ssl);
        if (r <= 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

    } else {

        /* clear (plain UDP): bound the receive so a lost or late reply fails
           the read instead of blocking forever -- the secured path above gets
           this via BIO_CTRL_DGRAM_SET_RECV_TIMEOUT. Windows takes the timeout
           as a DWORD in milliseconds */
        tv = 3000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    }

    return (fn); /* return fid */

}

ami_long ami_openmsgv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            ami_long port,
    /* link is secured */ ami_long secure
)

{

    struct sockaddr_in6 saddr;
    SOCKET sock;
    int fn;
    socket_struct laddr;
    int r;
    struct timeval timeout;
    DWORD tv;

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);
    saddr.sin6_port = htons(port);

    /* create the socket */
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    fn = msgfil(sock); /* park a fd entry on the socket */

    /* set up server address */
    memset(&opnfil[fn]->saddr.s6, 0, sizeof(struct sockaddr_in6));

    /* copy address information from caller */
    memcpy(&opnfil[fn]->saddr.s6, &saddr, sizeof(struct sockaddr_in6));

    opnfil[fn]->v6addr = TRUE; /* set v6 address */

    /* set up for DTLS operation if selected */
    if (secure) {

        /* clear local */
        memset((void *) &laddr, 0, sizeof(socket_struct));
        laddr.s6.sin6_family = AF_INET6;
        laddr.s6.sin6_port = htons(0);
        r = bind(sock, (const struct sockaddr *) &laddr,
                 sizeof(struct sockaddr_in6));
        if (r == SOCKET_ERROR) wskerr();

        /* create socket struct */
        ensure_client_dtls(); /* ensure context */
        opnfil[fn]->ssl = SSL_new(client_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        /* Create BIO, connect and set to already connected */
        opnfil[fn]->bio = BIO_new_dgram((int)sock, BIO_NOCLOSE);
        r = connect(sock, (struct sockaddr *) &opnfil[fn]->saddr,
                    sizeof(struct sockaddr_in6));
        if (r == SOCKET_ERROR) wskerr();

        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0,
                 &opnfil[fn]->saddr.ss);

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);

        r = SSL_connect(opnfil[fn]->ssl);
        if (r <= 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

    } else {

        /* clear (plain UDP): bound the receive so a lost or late reply fails
           the read instead of blocking forever */
        tv = 3000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    }

    return (fn); /* return fid */

}

/*******************************************************************************

Wait external message connection

Waits for an external message socket connection on a given port address. If an
external client connects to that port, then a socket file is opened and the file
number returned. Note that any number of such connections can be active at one
time. The program can invoke multiple tasks to handle each connection. If
another program tries to take the same port, it is blocked.

*******************************************************************************/

ami_long ami_waitmsg(/* port number to wait on */ ami_long port,
                /* secure mode */            ami_long secure
                )

{

    socket_struct caddr;
    SOCKET sock;
    int fn;
    int r;
    int opt;
    int off;
    struct timeval timeout;
    DWORD tv;

    /* create the socket */
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    /* Windows defaults v6 sockets to v6 only; turn that off so we serve v4
       clients as well (dual stack, matching Linux) */
    off = 0;
    setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));

    /* set socket options, multiple servers on address and same port */
    opt = 1;
    r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
                   sizeof(opt));
    if (r == SOCKET_ERROR) wskerr();

    fn = msgfil(sock); /* park a fd entry on the socket */

    /* clear server address */
    memset(&opnfil[fn]->saddr, 0, sizeof(socket_struct));

    /* set up address */
    opnfil[fn]->saddr.s6.sin6_family = AF_INET6;
    opnfil[fn]->saddr.s6.sin6_addr = in6addr_any;
    opnfil[fn]->saddr.s6.sin6_port = htons(port);

    opnfil[fn]->v6addr = TRUE; /* set v6 address */

    /* bind to socket */
    r = bind(sock, (struct sockaddr *)&opnfil[fn]->saddr.s6,
                 sizeof(struct sockaddr_in6));
    if (r == SOCKET_ERROR) wskerr();

    /* set up for DTLS operation if selected */
    if (secure) {

        memset(&caddr, 0, sizeof(socket_struct));

        /* Create BIO */
        opnfil[fn]->bio = BIO_new_dgram((int)sock, BIO_NOCLOSE);

        /* Set and activate timeouts */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        ensure_server_dtls(); /* ensure context */
        opnfil[fn]->ssl = SSL_new(server_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);
        SSL_set_options(opnfil[fn]->ssl, SSL_OP_COOKIE_EXCHANGE);

        while (DTLSv1_listen(opnfil[fn]->ssl, (BIO_ADDR *) &caddr) <= 0);

        /* Connect the listening socket itself to the client. Linux uses a
           second socket bound to the same port so it can keep listening, but
           Windows does not dispatch datagrams by connected tuple between
           sockets sharing a port, so the handshake would be delivered to the
           unread listening socket. We serve a single connection per wait, so
           the listening socket can simply become the connected one. */
        r = connect(sock, (struct sockaddr *) &caddr.s6,
                    sizeof(struct sockaddr_in6));
        if (r == SOCKET_ERROR) wskerr();

        /* set BIO to connected */
        BIO_ctrl(SSL_get_rbio(opnfil[fn]->ssl), BIO_CTRL_DGRAM_SET_CONNECTED,
                 0, &caddr.ss);

        /* Finish handshake */
        do { r = SSL_accept(opnfil[fn]->ssl); } while (r == 0);
        if (r < 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        BIO_ctrl(SSL_get_rbio(opnfil[fn]->ssl), BIO_CTRL_DGRAM_SET_RECV_TIMEOUT,
                 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

    } else {

        /* clear (plain UDP): bound the receive so a dropped client message
           (e.g. sent before this server bound) times out and the server exits
           instead of lingering. Windows takes the timeout as a DWORD in
           milliseconds */
        tv = 5000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    }

    return (fn); /* return fid */

}

/*******************************************************************************

Get maximum message size v4

Returns the maximum size of messages in the system. This is typically 1500, or
the maximum size of a UDP message, but can be larger, a so called "jumbo packet"
at 64kb, or somewhere in between.

We return the MTU reported by the interface. For a reliable network, this is
the absolute packet size. For others, it will be the MTU of the interface, which
packet breakage is possible.

Note the Windows loopback interface reports an effectively unbounded MTU, so
the result is clamped to the maximum UDP payload.

A message channel opened secure answers to the DTLS record before the route:
a datagram carries at most one record, and a record at most 16K of payload,
however large the route's packets run. With secure true the returned limit
honors that ceiling and the record's own framing; ask with the same secure
flag the channel will be opened with.

*******************************************************************************/

ami_long ami_maxmsg(
    /* ip address */      ami_ulong addr,
    /* link is secured */ ami_long secure
)

{

    struct sockaddr_in saddr;
    SOCKET sock;
    int r;
    int mtu;
    int mtulen;

    mtulen = sizeof(mtu); /* set length of word */

    /* set up target address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);

    /* create socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    /* connect to address */
    r = connect(sock, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r == SOCKET_ERROR) wskerr();

    /* Find the mtu. Unix reads the connected socket's path mtu with
       getsockopt(IP_MTU); Windows has no such option, so fall back to the
       standard ethernet mtu (the payload clamp below bounds it). A later
       refinement can read the real interface mtu through the IP helper API
       (GetBestInterfaceEx + GetIpInterfaceEntry). */
#ifdef IP_MTU
    r = getsockopt(sock, IPPROTO_IP, IP_MTU, (char*)&mtu, &mtulen);
    if (r == SOCKET_ERROR) wskerr();
#else
    (void)mtulen; mtu = 1500;
#endif

    closesocket(sock);

    /* The mtu includes the IP and UDP headers; the caller wants the largest
       message wrmsg can send. Also clamp to the largest possible UDP
       payload */
    mtu -= 28; /* IPv4 header 20 plus UDP header 8 */
    if (mtu > 65507) mtu = 65507;

    /* a secured channel is bounded by the DTLS record, not the route */
    if (secure) {

        mtu -= DTLSOVR; /* the record's framing rides inside the datagram */
        if (mtu > DTLSPAY) mtu = DTLSPAY;

    }
    if (mtu < 1) mtu = 1;

    return (mtu); /* return maximum message */

}

/*******************************************************************************

Get maximum message size v6

Returns the maximum size of messages in the system. This is typically 1500, or
the maximum size of a UDP message, but can be larger, a so called "jumbo packet"
at 64kb, or somewhere in between.

We return the MTU reported by the interface. For a reliable network, this is
the absolute packet size. For others, it will be the MTU of the interface, which
packet breakage is possible.

A message channel opened secure answers to the DTLS record before the route:
a datagram carries at most one record, and a record at most 16K of payload,
however large the route's packets run. With secure true the returned limit
honors that ceiling and the record's own framing; ask with the same secure
flag the channel will be opened with.

*******************************************************************************/

ami_long ami_maxmsgv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* link is secured */ ami_long secure
)

{

    struct sockaddr_in6 saddr;
    SOCKET sock;
    int r;
    int mtu;
    int mtulen;

    mtulen = sizeof(mtu); /* set length of word */

    /* set up target address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);

    /* create socket */
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) wskerr();

    /* connect to address */
    r = connect(sock, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r == SOCKET_ERROR) wskerr();

    /* find the mtu; see ami_maxmsg for why Windows uses the default */
#ifdef IPV6_MTU
    r = getsockopt(sock, IPPROTO_IPV6, IPV6_MTU, (char*)&mtu, &mtulen);
    if (r == SOCKET_ERROR) wskerr();
#else
    (void)mtulen; mtu = 1500;
#endif

    closesocket(sock);

    /* The mtu includes the IP and UDP headers; the caller wants the largest
       message wrmsg can send. Also clamp to the largest possible UDP
       payload */
    mtu -= 48; /* IPv6 header 40 plus UDP header 8 */
    if (mtu > 65527) mtu = 65527;

    /* a secured channel is bounded by the DTLS record, not the route */
    if (secure) {

        mtu -= DTLSOVR; /* the record's framing rides inside the datagram */
        if (mtu > DTLSPAY) mtu = DTLSPAY;

    }
    if (mtu < 1) mtu = 1;

    return (mtu); /* return maximum message */

}

/*******************************************************************************

Write message to message file

Writes a message to the given message file. The message file must be open. Any
size (including 0) up to ami_maxmsg() is allowed.

*******************************************************************************/

void ami_wrmsg(ami_long fn, void* msg, ami_ulong len)

{

    int r;
    int sr;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    if (opnfil[fn]->sudp) { /* secure udp */

        sr = SSL_write(opnfil[fn]->ssl, msg, len);
        if (sr <= 0) sslerror(opnfil[fn]->ssl, sr);

    } else {

        /* write the message to socket */
        if (opnfil[fn]->v6addr)
            r = sendto(opnfil[fn]->sock, msg, len, 0,
                       (const struct sockaddr *) &opnfil[fn]->saddr.s6,
                       sizeof(struct sockaddr_in6));
        else
            r = sendto(opnfil[fn]->sock, msg, len, 0,
                       (const struct sockaddr *) &opnfil[fn]->saddr.s4,
                       sizeof(struct sockaddr_in));
        if (r == SOCKET_ERROR) wskerr();

    }

}

/*******************************************************************************

Read message from message file

Reads a message from the message file, of any length up to and including the
specified buffer length. The actual length transferred is returned. The length
of the buffer should be equal to maxmsg to pass all possible messages, unless it
is known that a given message size will never be exceeded.

*******************************************************************************/
ami_long ami_rdmsg(ami_long fn, void* msg, ami_ulong len)

{

    int r;
    int al;
    int sr;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    if (opnfil[fn]->sudp) { /* secure udp */

        sr = SSL_read(opnfil[fn]->ssl, msg, len);
        if (sr <= 0) sslerror(opnfil[fn]->ssl, sr);
        r = sr; /* set length read */

    } else {

        /* read the message from socket, blocking (get full UDP message).
           The sender address is captured so a server can reply */
        if (opnfil[fn]->v6addr) {

            al = sizeof(struct sockaddr_in6);
            r = recvfrom(opnfil[fn]->sock, msg, len, 0,
                         (struct sockaddr *) &opnfil[fn]->saddr.s6, &al);

        } else {

            al = sizeof(struct sockaddr_in);
            r = recvfrom(opnfil[fn]->sock, msg, len, 0,
                         (struct sockaddr *) &opnfil[fn]->saddr.s4, &al);

        }
        if (r == SOCKET_ERROR) {

            /* Unix recvfrom truncates a datagram larger than the buffer and
               returns the buffer length; Windows copies the truncated data
               to the buffer the same way but then reports WSAEMSGSIZE. Give
               it the Unix behavior, which rdmsg is specified to have. */
            if (WSAGetLastError() == WSAEMSGSIZE) r = len;
            else wskerr();

        }

    }

    return (r); /* exit with length of message */

}

/*******************************************************************************

Test message file ready

Waits up to the given number of microseconds for a message to arrive on the
message file, zero to poll. Returns nonzero if a read would not block. A secure
channel is probed by the socket beneath it: the read that follows is what
decrypts.

*******************************************************************************/

ami_long ami_rdymsg(ami_long fn, ami_long usec)

{

    fd_set         fs;
    struct timeval tv;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    /* the file number is a parking place: the socket is the thing to probe */
    FD_ZERO(&fs);
    FD_SET(opnfil[fn]->sock, &fs);
    tv.tv_sec = (ami_long)(usec/1000000);
    tv.tv_usec = (ami_long)(usec%1000000);

    return (select(0, &fs, NULL, NULL, &tv) > 0);

}

/*******************************************************************************

Set message file timeout

Bounds the reads on the message file: a read that waits longer than the given
number of microseconds fails. Zero clears the bound, which is the setting for a
channel that is kept: the opens bound the reads by default so that a lost reply
fails instead of hanging.

*******************************************************************************/

void ami_tmomsg(ami_long fn, ami_long usec)

{

    DWORD ms;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    /* Windows takes the timeout as a DWORD in milliseconds */
    ms = (DWORD)((usec+999)/1000);
    setsockopt(opnfil[fn]->sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms,
               sizeof(ms));

}

/*******************************************************************************

Set message file receive buffer

Sizes the receive buffering on the message file, in bytes. A deep buffer lets a
burst of messages wait for a slow reader without loss.

*******************************************************************************/

void ami_bufmsg(ami_long fn, ami_long len)

{

    int rb = (int)len;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    setsockopt(opnfil[fn]->sock, SOL_SOCKET, SO_RCVBUF, (const char*)&rb,
               sizeof(rb));

}

/*******************************************************************************

Shut down message file

Shuts the message file under its reader: a read blocked on it, in another
thread, fails. The file still wants closing.

*******************************************************************************/

void ami_shutmsg(ami_long fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    shutdown(opnfil[fn]->sock, SD_BOTH);

}

/*******************************************************************************

Close message file

Closes the given message file.

*******************************************************************************/

void ami_clsmsg(ami_long fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn] || !opnfil[fn]->msg) error(enotmsg);

    /* If DTLS, send the close notify while the socket is still open, then
       free the ssl struct (frees the attached BIO as well) */
    if (opnfil[fn]->sudp) SSL_shutdown(opnfil[fn]->ssl);
    if (opnfil[fn]->sudp) SSL_free(opnfil[fn]->ssl);
    opnfil[fn]->ssl = NULL;
    opnfil[fn]->bio = NULL;

    closesocket(opnfil[fn]->sock); /* close the socket */
    if (opnfil[fn]->sock2 != INVALID_SOCKET) closesocket(opnfil[fn]->sock2);
    opnfil[fn]->sock = INVALID_SOCKET;
    opnfil[fn]->sock2 = INVALID_SOCKET;
    opnfil[fn]->net = FALSE;
    opnfil[fn]->msg = FALSE;
    opnfil[fn]->sudp = FALSE;

    _close((int)fn); /* release the parked fd */

}

/*******************************************************************************

Wait external network connection

Waits for an external socket connection on a given port address. If an external
client connects to that port, then a socket file is opened and the file number
returned. Note that any number of such connections can be active at one time.
The program can invoke multiple tasks to handle each connection. If another
program tries to take the same port, it is blocked.

*******************************************************************************/

FILE* ami_waitnet(/* port number to wait on */ ami_long port,
                 /* secure mode */            ami_long secure
                )

{

    struct sockaddr_in6 saddr;
    SOCKET ls, as;
    int r;
    int opt;
    int off;

    /* create the listening socket */
    ls = socket(AF_INET6, SOCK_STREAM, 0);
    if (ls == INVALID_SOCKET) wskerr();

    /* Windows defaults v6 sockets to v6 only; turn that off so we serve v4
       clients as well (dual stack, matching Linux) */
    off = 0;
    setsockopt(ls, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));

    /* set socket options, multiple servers on address and same port */
    opt = 1;
    r = setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
                   sizeof(opt));
    if (r == SOCKET_ERROR) wskerr();

    /* set up address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_addr = in6addr_any;
    saddr.sin6_port = htons(port);
    r = bind(ls, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r == SOCKET_ERROR) wskerr();

    /* wait on port */
    r = listen(ls, 3);
    if (r == SOCKET_ERROR) wskerr();

    /* accept connection, discard peer address */
    as = accept(ls, NULL, NULL);
    if (as == INVALID_SOCKET) wskerr();

    /* discard server port */
    closesocket(ls);

    /* finish with general routine */
    return (sockfil(as, secure, TRUE));

}

/*******************************************************************************

Message files are reliable

Returns true if the message files on this host are implemented with guaranteed
delivery and in order. This is a property of high performance compute clusters,
that the delivery of messages are guaranteed to arrive error free at their
destination. If this property appears, the program can skip providing it's own
retry or other error handling system.

There are two versions of this routine, depending on if ipv4 or ipv6 addresses
are used.

At the moment, this is a test to see if the address is the local address of
the host, meaning that it is a completely local connection that will not be
carried on the wire. Thus it is reliable by definition.

*******************************************************************************/

ami_long ami_relymsg(ami_ulong addr)

{

    return (addr == 0x7f000001);

}

ami_long ami_relymsgv6(unsigned long long addrh, unsigned long long addrl)

{

    return (addrh == 0 && addrl == 1); /* test local host */

}

/*******************************************************************************

Get message certificate

Fetches the SSL certificate for an DTLS connection. The file must contain
an open and active DTLS connection. Retrieves on of the indicated
certificates by number, from 1 to N where N is the maximum certificate in the
chain. Certificate 1 is the certificate for the server connected. Certificate
N is the CA or Certificate Authority's certificate, AKA the root certificate.
The size of the certificate buffer is passed, and the actual length of the
certificate is returned. If there is no certificate by a given number, the
resulting length is zero.

Certificates are in base64 format, the same as a PEM file, starting with the
line "-----BEGIN CERTIFICATE-----" and ending with the line
"-----END CERTIFICATE----". The buffer contains a whole
certificate. Note that characters with value < 20 (control characters), may or
may not be included in the certificate. Typically carriage returns, line feed
or both may be used to break up lines in the certificate.

Certificates are normally retrieved in numerical order, that is, 1, 2, 3...N.
Thus the end of the certificate chain must be found by traversal.

The certificate buffer is a critical buffer: a result that fills the entire
buffer is left without a terminating zero, a shorter result is zero
terminated, and it is an error if the certificate cannot fit in the buffer.

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

ami_long ami_certmsg(ami_long fn, ami_long which, string buff, ami_long len)

{

    X509* cert;
    X509* peer;
    STACK_OF(X509)* certstk;
    int ownstk;
    BIO* cb;
    int r;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (which < 1) error(einvctn); /* invalid certificate number */

    makfil(fn); /* create file entry as required */
    if (!opnfil[fn]->sudp && !opnfil[fn]->sec) error(enotsec);

    /* get the certificate */
    peer = SSL_get_peer_certificate(opnfil[fn]->ssl);
    if (!peer) error(enocert);
    cert = peer;
    certstk = NULL;
    ownstk = FALSE;

    /* see if we need to get the chain */
    if (which > 1) {

        certstk = SSL_get_peer_cert_chain(opnfil[fn]->ssl);
        if (!certstk) {

            /* Zakir Durumeric says that it is possible to get a null chain,
               even though obviously a single certificate exists (it should
               be first in chain). So if this happens, we create our own chain
               and stuff the first certificate in it */
            certstk = sk_X509_new_null();
            sk_X509_push(certstk, cert);
            ownstk = TRUE; /* we own this stack shell */

        }
        /* if the certificate number is out of range, return nothing */
        if (which > sk_X509_num(certstk)) cert = NULL;
        else cert = sk_X509_value(certstk, (int)(which-1));

    }

    r = 0; /* set no certificate */
    if (cert) { /* there was a certificate, read to memory */

        /* make a bio to memory */
        cb = BIO_new(BIO_s_mem());
        if (!cb) error(enobio);

        /* write certificate to BIO */
        r = PEM_write_bio_X509(cb, cert);
        if (!r) error(ewrbio);

        /* read certificate back to memory */
        r = BIO_read(cb, buff, (int)len);
        if (r < 0) error(erdbio);
        if (!BIO_eof(cb)) error(ecerttl);

        BIO_free(cb); /* release the conversion BIO */

    }
    /* release the chain shell if we made it, and our reference to the peer
       certificate (chain members are internal references, not freed) */
    if (ownstk) sk_X509_free(certstk);
    X509_free(peer);

    /* the certificate buffer is a critical buffer: terminate the result
       only if it does not fill the entire buffer */
    if (r < len) buff[r] = 0;

    return (r);

}

/*******************************************************************************

Get network certificate

Fetches the SSL certificate for an SSL connection. The file must contain an
open and active SSL connection. Retrieves on of the indicated certificates by
number, from 1 to N where N is the maximum certificate in the chain.
Certificate 1 is the certificate for the server connected. Certificate N is the
CA or Certificate Authority's certificate, AKA the root certificate. The size
of the certificate buffer is passed, and the actual length of the certificate
is returned. If there is no certificate by a given number, the resulting length
is zero.

Certificates are in base64 format, the same as a PEM file, except without the
"BEGIN CERTIFICATE" and "END CERTIFICATE" lines. The buffer contains a whole
certificate. Note that characters with value < 20 (control characters), may or
may not be included in the certificate. Typically carriage returns, line feed
or both may be used to break up lines in the certificate.

Certificates are normally retrieved in numerical order, that is, 1, 2, 3...N.
Thus the end of the certificate chain must be found by traversal.

The certificate buffer is a critical buffer: a result that fills the entire
buffer is left without a terminating zero, a shorter result is zero
terminated, and it is an error if the certificate cannot fit in the buffer.

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

ami_long ami_certnet(FILE* f, ami_long which, string buff, ami_long len)

{

    int fn;

    fn = fileno(f); /* get fid */

    /* with the fid, the call is the same as for messaging */
    return (ami_certmsg(fn, which, buff, len));

}

/*******************************************************************************

Get network certificate data list

Retrieves a list of data fields from the given file by number. The file
must contain an open and active SSL connection. The data list is a list of
name - data pairs, both strings. The list can also have branches or forks,
which make it able to contain complete trees. The certificate number is from
1 to N where N is the maximum certificate in the chain. Certificate 1 is the
certificate for the server connected. Certificate N is the CA or Certificate
Authority's certificate, AKA the root certificate. If there is no certificate
by that number, the resulting list is NULL.

Certificates are normally retrieved in numerical order, that is, 1, 2, 3...N.
Thus the end of the certificate chain must be found by traversal.

Note that the list is allocated by this routine, and the caller is responsible
for freeing the list as necessary.

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

The formatting and tree structure mostly follows OpenSSL formatting. For
example, the root is labeled "certificate" even if that it is fairly redundant,
and is easy to prune off.

*******************************************************************************/

/* put contents of bio in buffer */

static int getbio(BIO *bp, char* buff, int len)

{

    int r;

    do {

        /* read data to buffer */
        r = BIO_gets(bp, buff, len);
        if (r < 0) error(erdbio);
        buff += r; /* advance pointers */
        len -= r;
        if (!len) error(ebufovf);

    } while (r > 0);

    return (r); /* return length of buffer content */

}

/* make certificate node */

static ami_certptr maknode(string name)

{

    ami_certptr p;

    p = getcert(); /* get a new certificate d/v entry */
    p->name = malloc(strlen(name)+1); /* get string entry for name */
    strcpy(p->name, name); /* copy into place */

    return (p);

}

/* fill data value */

static void filldata(ami_certptr cp, const char* value)

{

    cp->data = malloc(strlen(value)+1); /* get string entry for value */
    strcpy(cp->data, value); /* copy into place */

}

/* add new entry to end of list */

static ami_certptr addend(ami_certptr* list, string name)

{

    ami_certptr p, p2;

    p = maknode(name); /* create entry */
    /* append to end of list */
    if (*list) { /* there are entries */

       p2 = *list; /* index top of list */
       /* find end of list */
       while (p2->next) { p2 = p2->next; }
       /* append */
       p2->next = p;

    } else *list = p; /* set as root */

    return (p);

}

/* get line from buffer (including '\n') */

static void getlin(char** ibuff, char** obuff)

{

    /* move everything before end of line */
    while (**ibuff && **ibuff != '\n') *(*obuff)++ = *(*ibuff)++;
    /* move end of line */
    if (**ibuff == '\n') { *(*obuff)++ = '\n'; (*ibuff)++; }
    **obuff = 0; /* terminate */

}

/* remove any last \n on line */

static void remeol(char* buff)

{

    char* cp;

    cp = NULL; /* set no last */
    /* find end */
    while (*buff) { cp = buff; buff++; }
    /* if \n is last character, knock it out */
    if (cp && *cp == '\n') *cp = 0;

}

/*

Find key

Finds a key of the form:

key<sp>:

The key must start with an alpha character, and must be longer than 2
characters. That requirement comes from the appearance of hex 'xx' data bytes
in the input. Leading spaces and spaces between the name and ':' are skipped,
but spaces within the key are kept.

*/

static void fndkey(char buff[], char key[], char** ncp)

{

    char *icp, *ocp;

//dbg_printf(dlinfo, "fndkey: buff: %s\n", buff);
    key[0] = 0; /* clear output key */
    icp = buff; /* index first character */
    ocp = key; /* index output buffer */
    *ncp = icp; /* set new position at buffer start */
    while (isspace(*icp)) icp++; /* skip leading spaces */
    if (isalpha(*icp)) { /* found leader */

        /* place whole key in buffer */
        while (*icp && *icp != ':') *ocp++ = *icp++;
        *ocp = 0; /* terminate key */
        /* if not found kill the key */
        if (*icp != ':') key[0] = 0;
        else icp++; /* otherwise skip ':' */
        /* if too short kill the key */
        if (ocp-key <= 2) key[0] = 0;

    }
    /* skip trailing spaces */
    while (isspace(*icp)) icp++;
    /* check key is simple descriptor that we don't want expanded into a key */
    if (!strcmp(key, "keyid") || !strcmp(key, "DNS") || !strcmp(key, "URI"))
       key[0] = 0; /* kill the key */
    if (key[0]) *ncp = icp; /* set new position after key */
//dbg_printf(dlinfo, "fndkey: key: %s remaining: %s\n", key, icp);

}

/*

Get name/value series from buffer.

This is used to parse buffers where the component cert key printers are
not accessible from outside OpenSSL. Note we rely on names being flush
left. Each field is of the form:

name: value

Terminated by \n. Values can either terminate on the same line, or occupy
multiple lines. In that case, each subsequent line is indented by one or more
spaces. For these values, the indentation is removed, and the entire value
concatenated, with the \n line endings left intact.

Content that has no key at all (as in extensions like the authority key
identifier, whose entire body is "keyid:...", or key usage, which is plain
text) is placed as the value of the orphan entry, normally the entry the
list forks from. With no orphan entry given, keyless content is a parse
error.

*/

static void getnamval(char* buff, ami_certptr* list, ami_certptr orphan)

{

    char lbuff[CVBUFSIZ]; /* line output buffer */
    char vbuff[CVBUFSIZ]; /* value output buffer */
    char name[1024]; /* name */
    char* cp; /* output buffer pointer */
    ami_certptr cdp; /* current name/val being worked on */

//dbg_printf(dlinfo, "getnamval: buff:\n%s\n", buff);
    cdp = orphan; /* start on the orphan entry, if given */
    vbuff[0] = 0; /* terminate empty value */
    while (*buff) { /* loop over all lines in buffer */

        cp = lbuff; /* index output buffer */
        getlin(&buff, &cp); /* next next line */
        /* try to match a key */
        fndkey(lbuff, name, &cp);
        if (name[0]) {

//dbg_printf(dlinfo, "getnamval: key found: %s\n", name);
            /* terminate any outstanding entry */
            remeol(vbuff); /* remove last \n, if exists */
            /* place gathered data as value; the orphan entry is only filled
               if it actually gathered content */
            if (cdp && (cdp != orphan || vbuff[0]))
                filldata(cdp, vbuff);
            cdp = addend(list, name); /* create n/v entry */
            vbuff[0] = 0; /* terminate empty value */
            if (*cp && *cp != '\n')
                /* there is more on the line, add to value */
                strcat(vbuff, cp); /* concatenate value */

        } else { /* no key, concatenate to value */

            /* does not begin with key, and there is no working entry and no
               orphan entry to give the content to, then something is wrong */
            if (!cdp) error(ecertpar);
            strcat(vbuff, cp); /* concatenate to value */

        }

    }
    /* terminate any outstanding entry */
    remeol(vbuff); /* remove last \n, if exists */
    if (cdp && (cdp != orphan || vbuff[0]))
        filldata(cdp, vbuff); /* place gathered data as value */

}

/*******************************************************************************

Get message certificate data list

Retrieves a list of data fields from the given file by number. The file
must contain an open and active TLS or DTLS connection. The data list is a
list of name - data pairs, both strings. The list can also have branches or
forks, which make it able to contain complete trees. The certificate number
is from 1 to N where N is the maximum certificate in the chain. Certificate 1
is the certificate for the server connected. Certificate N is the CA or
Certificate Authority's certificate, AKA the root certificate. If there is no
certificate by that number, the resulting list is NULL.

Certificates are normally retrieved in numerical order, that is, 1, 2, 3...N.
Thus the end of the certificate chain must be found by traversal.

Note that the list is allocated by this routine, and the caller is
responsible for freeing the list as necessary (see ami_certlistfree).

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

void ami_certlistmsg(ami_long fn, ami_long which, ami_certptr* list)

{

    X509* cert;
    X509* peer;
    STACK_OF(X509)* certstk;
    int ownstk;
    BIO* bp;
    int r;
    char* cp;
    int v;
    ASN1_INTEGER* sn;
    BIGNUM* bn;
    int i, j, l;
    ASN1_TIME* atp;
    char buff[CVBUFSIZ];
    X509_EXTENSION* ep;
    ASN1_OBJECT* op;
    X509_PUBKEY* kp;
    EVP_PKEY* ekp;
    const X509_ALGOR *sig_alg;
    const ASN1_BIT_STRING *sig;
    const ASN1_OBJECT *sao;
    const unsigned char* sd;
    /* branch placeholders */
    ami_certptr certificate;
    ami_certptr data;
    ami_certptr sigal;
    ami_certptr validity;
    ami_certptr extensions;
    ami_certptr cdp;

    *list = NULL; /* set no result */
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (which < 1) error(einvctn); /* invalid certificate number */

    makfil(fn); /* create file entry as required */
    if (!opnfil[fn]->sudp && !opnfil[fn]->sec) error(enotsec);

    /* get the certificate */
    peer = SSL_get_peer_certificate(opnfil[fn]->ssl);
    if (!peer) error(enocert);
    cert = peer;
    certstk = NULL;
    ownstk = FALSE;

    /* see if we need to get the chain */
    if (which > 1) {

        certstk = SSL_get_peer_cert_chain(opnfil[fn]->ssl);
        if (!certstk) {

            /* Zakir Durumeric says that it is possible to get a null chain,
               even though obviously a single certificate exists (it should
               be first in chain). So if this happens, we create our own chain
               and stuff the first certificate in it */
            certstk = sk_X509_new_null();
            sk_X509_push(certstk, cert);
            ownstk = TRUE; /* we own this stack shell */

        }
        /* if the certificate number is out of range, return nothing */
        if (which > sk_X509_num(certstk)) cert = NULL;
        else cert = sk_X509_value(certstk, (int)(which-1));

    }

    if (cert) {

        /* get memory BIO to convert output */
        bp = BIO_new(BIO_s_mem());

        /* make the top forks */
        certificate = addend(list, "Certificate"); /* make root */
        data = addend(&certificate->fork, "Data"); /* make data branch */

        /* Get the different certificate fields. We use openssl's certificate
           prints as a guide for formatting */

        cdp = addend(&data->fork, "Version");
        v = X509_get_version(cert)+1;
        sprintf(buff, "%d", v);
        filldata(cdp, buff);

        cdp = addend(&data->fork, "Serial Number");
        sn = X509_get_serialNumber(cert);
        bn = ASN1_INTEGER_to_BN(sn, NULL);
        if (!bn) error(enoserl);
        cp = BN_bn2hex(bn);
        if (!cp) error(enoserl);
        l = strlen(cp);
        j = 0;
        for (i = 0; i < l; i++)
            { buff[j++] = *cp++; if (i % 2 && i < l-1) buff[j++] = ':'; }
        buff[j] = 0;
        filldata(cdp, buff);

        cdp = addend(&data->fork, "Signature Algorithm");
        i = X509_get_signature_nid(cert);
        if (i == NID_undef) error(enosiga);
        const char* sa = OBJ_nid2ln(i);
        filldata(cdp, sa);

        cdp = addend(&data->fork, "Issuer");
        X509_NAME_print_ex(bp, X509_get_issuer_name(cert), 0, 0/*XN_FLAG_SPC_EQ*/);
        getbio(bp, buff, CVBUFSIZ);
        filldata(cdp, buff);

        /* start subfork */
        validity = addend(&data->fork, "Validity");

        cdp = addend(&validity->fork, "Not Before");
        atp = X509_get_notBefore(cert);
        r = ASN1_TIME_print(bp, atp);
        if (r <= 0) error(ewrbio);
        getbio(bp, buff, CVBUFSIZ);
        filldata(cdp, buff);

        cdp = addend(&validity->fork, "Not After");
        atp = X509_get_notAfter(cert);
        r = ASN1_TIME_print(bp, atp);
        if (r <= 0) error(ewrbio);
        getbio(bp, buff, CVBUFSIZ);
        filldata(cdp, buff);

        cdp = addend(&data->fork, "Subject");
        X509_NAME_print_ex(bp, X509_get_subject_name(cert), 0, 0/*XN_FLAG_SPC_EQ*/);
        getbio(bp, buff, CVBUFSIZ);
        filldata(cdp, buff);

        cdp = addend(&data->fork, "Subject Public Key Info");
        cdp = addend(&cdp->fork, "Public Key Algorithm");
        kp = X509_get_X509_PUBKEY(cert); /* get public key */
        /* get subject public key info */
        X509_PUBKEY_get0_param(&op, NULL, NULL, NULL, kp);
        i2a_ASN1_OBJECT(bp, op);
        getbio(bp, buff, CVBUFSIZ);
        filldata(cdp, buff);

        /* This one we have to take apart, the routines are buried in OpenSSL */
        ekp = X509_get0_pubkey(cert);
        EVP_PKEY_print_public(bp, ekp, 0, NULL);
        getbio(bp, buff, CVBUFSIZ); /* place in buffer */
        getnamval(buff, &cdp->fork, cdp); /* parse n/v tree */

        extensions = addend(&data->fork, "X509v3 extensions");
        const STACK_OF(X509_EXTENSION)* esp = X509_get0_extensions(cert);
        l = X509v3_get_ext_count(esp);
        for (i = 0; i < l; i++) {

            ep = sk_X509_EXTENSION_value(esp, i);
            op = X509_EXTENSION_get_object(ep);
            i2a_ASN1_OBJECT(bp, op);
            getbio(bp, buff, CVBUFSIZ);
            cdp = addend(&extensions->fork, buff);
//dbg_printf(dlinfo, "Extension key: %s\n", buff);
            cdp->critical = X509_EXTENSION_get_critical(ep);
            //r = ssl_X509V3_EXT_print(bp, ep, 0);
            r = X509V3_EXT_print(bp, ep, 0, 0);
            /* these appear all empty in practice */
            if (!r) ASN1_STRING_print(bp, X509_EXTENSION_get_data(ep));
            getbio(bp, buff, CVBUFSIZ);
//dbg_printf(dlinfo, "Extension data: <start>\n%s\n<end>\n", buff);
            //filldata(cdp, buff);
            getnamval(buff, &cdp->fork, cdp); /* parse n/v tree */

        }

        /* The outer (certificate level) signature. X509_signature_print()
           formats these with indentation that fights the name/value parser,
           so take the structure apart directly: X509_get0_signature() gives
           the algorithm (an X509_ALGOR, whose object is extracted with
           X509_ALGOR_get0) and the signature bit string (an ASN1_BIT_STRING,
           read with ASN1_STRING_get0_data/ASN1_STRING_length) */
        sigal = addend(&certificate->fork, "Signature Algorithm");
        X509_get0_signature(&sig, &sig_alg, cert);
        X509_ALGOR_get0(&sao, NULL, NULL, sig_alg);
        i = OBJ_obj2nid(sao);
        if (i != NID_undef) filldata(sigal, (char*)OBJ_nid2ln(i));
        else { /* not a known algorithm, give the raw object id */

            i2a_ASN1_OBJECT(bp, (ASN1_OBJECT*)sao);
            getbio(bp, buff, CVBUFSIZ);
            filldata(sigal, buff);

        }

        cdp = addend(&certificate->fork, "Signature Value");
        sd = ASN1_STRING_get0_data(sig);
        l = ASN1_STRING_length(sig);
        /* format as colon separated hex, as openssl prints it */
        if (l > (CVBUFSIZ-1)/3) l = (CVBUFSIZ-1)/3; /* clamp to buffer */
        j = 0;
        for (i = 0; i < l; i++) {

            j += sprintf(&buff[j], "%02x", sd[i]);
            if (i < l-1) buff[j++] = ':';

        }
        buff[j] = 0;
        filldata(cdp, buff);

        BIO_free(bp); /* release the conversion BIO */

    }
    /* release the chain shell if we made it, and our reference to the peer
       certificate (chain members are internal references, not freed) */
    if (ownstk) sk_X509_free(certstk);
    X509_free(peer);

}

/*******************************************************************************

Get network certificate data list

Retrieves a list of data fields from the given file by number. The file must
contain an open and active TLS connection. See the notes for
ami_certlistmsg, which this routine wraps.

*******************************************************************************/

void ami_certlistnet(FILE *f, ami_long which, ami_certptr* list)

{

    ami_certlistmsg(fileno(f), which, list); /* execute with fid */

}

/*******************************************************************************

Free certificate data list

Frees a certificate data list as returned by ami_certlistnet or
ami_certlistmsg, including all branches and strings. The list pointer is set
to NULL.

*******************************************************************************/

void ami_certlistfree(ami_certptr* list)

{

    ami_certptr p;

    while (*list) { /* traverse the top level list */

        p = *list; /* top entry from list */
        *list = p->next;
        putcert(p); /* free entry and its tree */

    }

}

/*******************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
lseek

We use interdiction to allow network files to use stdio calls on network
streams, but allow enhanced features to be implemented as well.

*******************************************************************************/

/*******************************************************************************

Open

We don't do anything for this, so pass it on.

*******************************************************************************/

static int iopen(const char* pathname, int flags, int perm)

{

    return (*ofpopen)(pathname, flags, perm);

}

/*******************************************************************************

Close file

Closes the file. The file is closed at the system level, then we remove the
table entry for the file.

A file can be a normal file or a network connection. If its normal, it is passed
on to the operating system. If it's a network connection, then the network
connection is broken, and both the input and output sides are closed.

*******************************************************************************/

static int iclose(int fd)

{

    filptr fp;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->net) {

        fp = opnfil[fd]; /* index that */
        /* send the close notify while the socket is still open */
        if (fp->ssl) SSL_shutdown(fp->ssl);
        if (fp->ssl) SSL_free(fp->ssl); /* free the ssl */
        fp->ssl = NULL;
        fp->bio = NULL; /* freed with the ssl */
        if (fp->cert) X509_free(fp->cert); /* free certificate */
        fp->cert = NULL;
        if (fp->sock != INVALID_SOCKET) closesocket(fp->sock);
        if (fp->sock2 != INVALID_SOCKET) closesocket(fp->sock2);
        fp->sock = INVALID_SOCKET;
        fp->sock2 = INVALID_SOCKET;
        fp->net = FALSE;
        fp->sec = FALSE;
        fp->msg = FALSE;
        fp->sudp = FALSE;

    }

    return (*ofpclose)(fd); /* normal file close */

}

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

Note that on sockets a short read is normal; the stdio layer above simply
takes what arrives. A zero result (connection closed by the peer) is passed
through as end of file.

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    int r; /* int result */
    int e;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->net) {

        if (opnfil[fd]->sec) { /* secure (TLS) file */

            r = SSL_read(opnfil[fd]->ssl, buff, count);
            if (r <= 0) {

                e = SSL_get_error(opnfil[fd]->ssl, r);
                /* orderly shutdown by peer is end of file */
                if (e == SSL_ERROR_ZERO_RETURN) r = 0;
                else sslerror(opnfil[fd]->ssl, r);

            }

        } else {

            r = recv(opnfil[fd]->sock, buff, count, 0); /* receive network data */
            if (r == SOCKET_ERROR) wskerr(); /* flag read error */

        }

        return (r);

    }

    return (*ofpread)(fd, buff, count); /* standard file read */

}

/*******************************************************************************

Write file

Writes a byte buffer to the output file. If the file is normal, we pass it on.
If the file is a network file, we process a write on the associated socket.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    int r;
    const char* p;
    size_t left;

    if (fd >= 0 && fd < MAXFIL && opnfil[fd] && opnfil[fd]->net) {

        if (opnfil[fd]->sec) { /* secure (TLS) file */

            r = SSL_write(opnfil[fd]->ssl, buff, count);
            if (r <= 0) sslerror(opnfil[fd]->ssl, r);

        } else {

            /* transmit network data; send may take less than the whole
               buffer, so loop until everything is out */
            p = buff;
            left = count;
            while (left) {

                r = send(opnfil[fd]->sock, p, left, 0);
                if (r == SOCKET_ERROR) wskerr(); /* flag write error */
                p += r;
                left -= r;

            }

        }

        return (count);

    }

    /* standard file write */
    return (*ofpwrite)(fd, buff, count);

}

/*******************************************************************************

Lseek

Lseek is never possible on a network, so this just passed on.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    return (*ofplseek)(fd, offset, whence);

}

/*******************************************************************************

Console control handler

This procedure gets activated as a callback when Windows flags a termination
event to console. We immediately abort.

At the present time, we don't care what type of termination event it was,
all generate an etterm signal.

*******************************************************************************/

static BOOL WINAPI conhan(DWORD ct)

{

    int fi; /* index for file tables */

    WSACleanup();
    /* shutdown the open connections here, because the winsock dll is already
       shut down before the deinit handler gets executed */
    if (!dblflt) { /* we haven't already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->net &&
                opnfil[fi]->sock != INVALID_SOCKET)
                closesocket(opnfil[fi]->sock); /* close socket */

    }

    return (1); /* set event handled */

}

/*******************************************************************************

Network startup

*******************************************************************************/

static void ami_init_network (void) __attribute__((constructor (103)));
static void ami_init_network()

{

    int     fi;  /* index for file tables */
    int     r;   /* result code */
    WSADATA wsd; /* windows socket data */

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);

    dblflt = FALSE; /* set no double fault */

    /* clear open files table */
    for (fi = 0; fi < MAXFIL; fi++) opnfil[fi] = NULL; /* set unoccupied */

    /* initialize the table/context lock */
    InitializeCriticalSection(&netlck);

    /* perform winsock startup */
    r = WSAStartup(MAKEWORD(2,2), &wsd);
    if (r) wskerr(); /* can't initalize Winsock */

    /* initialize SSL library and register algorithms */
    if(SSL_library_init() < 0) error(einissl);
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();

    /* The four SSL contexts are created lazily on first secure use (see the
       ensure_client_tls/ensure_client_dtls/ensure_server_tls/
       ensure_server_dtls functions above), so the certificate/key .pem files
       are not required by a program that does no secure networking. */

    /* set cookie uninitialized */
    cookie_initialized = FALSE;

    /* capture control handler so that ctl-c cancels properly. This is a
       workaround because the winsock dll gets shutdown before we reach
       the deinit function. */
    //SetConsoleCtrlHandler(conhan, TRUE);

}

/*******************************************************************************

Network shutdown

*******************************************************************************/

static void ami_deinit_network (void) __attribute__((destructor (103)));
static void ami_deinit_network()

{

    /* holding copies of system vectors */
    pread_t cppread;
    pwrite_t cppwrite;
    popen_t cppopen;
    pclose_t cppclose;
    plseek_t cpplseek;

    int fi; /* index for file tables */

    if (!dblflt) { /* we haven't already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->net) {

                /* politely notify any live peer before the free */
                if (opnfil[fi]->ssl) SSL_shutdown(opnfil[fi]->ssl);
                if (opnfil[fi]->ssl) SSL_free(opnfil[fi]->ssl);
                if (opnfil[fi]->cert) X509_free(opnfil[fi]->cert);
                if (opnfil[fi]->sock != INVALID_SOCKET)
                    closesocket(opnfil[fi]->sock); /* close socket */
                if (opnfil[fi]->sock2 != INVALID_SOCKET)
                    closesocket(opnfil[fi]->sock2);

            }

    }
    /* free context structures */
    if (client_tls_ctx) SSL_CTX_free(client_tls_ctx);
    if (client_dtls_ctx) SSL_CTX_free(client_dtls_ctx);
    if (server_tls_ctx) SSL_CTX_free(server_tls_ctx);
    if (server_dtls_ctx) SSL_CTX_free(server_dtls_ctx);

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose || cpplseek != ilseek)
        error(esystem);

    DeleteCriticalSection(&netlck);

    /* release control handler */
    SetConsoleCtrlHandler(NULL, FALSE);

}
