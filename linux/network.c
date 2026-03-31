/*******************************************************************************
*                                                                              *
*                           INTERNET ACCESS LIBRARY                            *
*                                                                              *
*                       Copyright (C) 2019 Scott A. Franco                     *
*                                                                              *
* Implements access to internet functions, via tcp/ip. tcp/ip is implemented   *
* via the "file" paradygm. An address and port is used to create a file, then  *
* normal stdio read and write statements are used to access it.                *
*                                                                              *
* Linux (as well as all socket based Unix clones), already treats connections  *
* as files. What the network library does for you is make it portable to       *
* systems that cannot do that (Windows), and also give a standard method to    *
* open such files.                                                             *
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

/* Linux definitions */
#include <sys/types.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

/* Petit-Ami definitions */
#include <localdefs.h>
#include <network.h>

#include <diag.h>

#ifndef __MACH__ /* Mac OS X */
#define NOCANCEL /* include nocancel overrides */
#endif

#define MAXFIL 100 /* maximum number of open files */
#define COOKIE_SECRET_LENGTH 16 /* length of secret cookie */
#define CVBUFSIZ 4096 /* certificate value buffer size */

/* this is missing from Mac OS X */
#ifdef __MACH__ /* Mac OS X */
#define IP_MTU 14 /* int */
#endif

/* socket structures */
typedef union {

    struct sockaddr_storage ss;
    struct sockaddr_in6 s6;
    struct sockaddr_in s4;

} socket_struct;

/* File tracking.
   Files could be transparent in the case of plain text, but SSL and advanced
   layering needs special handling. So we translate the file descriptors and
   flag if they need special handling. */
typedef struct filrec {

   pthread_mutex_t lock; /* lock for this structure */
   /* we don't use the network flag for much right now, except to indicate that
      the file is a socket. Linux treats them equally */
   int             net;  /* it's a network file */
   int             sec;  /* its a secure sockets file */
   SSL*            ssl;  /* SSL data */
   X509*           cert; /* peer certificate */
   int             sfn;  /* shadow fid */
   int             opn;  /* file is open with Linux */
   int             msg;  /* is a message socket (udp/dtls) */
   socket_struct   saddr; /* socket address */
   int             v6addr; /* is an IPv6 address */
   BIO*            bio;  /* bio for DTLS */
   int             sudp; /* its a secure udp */
   struct filrec*  next; /* next entry (when placed on free list) */


} filrec;
typedef filrec* filptr; /* pointer to file record */

/* error codes */
typedef enum {

    ewskini,  /* cannot initialize winsock */
    einvhan,  /* invalid file handle */
    enetopn,  /* cannot reset or rewrite network file */
    enetpos,  /* cannot position network file */
    enetloc,  /* cannot find location network file */
    enetlen,  /* cannot find length network file */
    esckeof,  /* end encountered on socket */
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
    enoext,   /* Could not get certificate extension */
    enomem,   /* Cannot allocate memory */
    ebufovf,  /* Buffer overflow */
    ecertpar, /* Error parsing certificate data */
    eunimp,   /* Feature not implemented */
    esystem   /* System consistency check */

} errcod;

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int (*popen_t)(const char*, int, int);
typedef int (*pclose_t)(int);
typedef off_t (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

#ifdef NOCANCEL
extern void ovr_read_nocancel(pread_t nfp, pread_t* ofp);
extern void ovr_write_nocancel(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open_nocancel(popen_t nfp, popen_t* ofp);
extern void ovr_close_nocancel(pclose_t nfp, pclose_t* ofp);
#endif

/*
 * Saved vectors to system calls. These vectors point to the old, existing
 * vectors that were overriden by this module.
 *
 */
static pread_t   ofpread;
static pread_t   ofpread_nocancel;
static pwrite_t  ofpwrite;
static pwrite_t  ofpwrite_nocancel;
static popen_t   ofpopen;
static popen_t   ofpopen_nocancel;
static pclose_t  ofpclose;
static pclose_t  ofpclose_nocancel;
static plseek_t  ofplseek;

/*
 * Variables
 */
static filptr opnfil[MAXFIL];  /* open files table */
static pthread_mutex_t oflock; /* lock for this structure */
static filptr frefil;          /* free file entries list */
static pthread_mutex_t fflock; /* lock for this structure */
static ami_certptr frecert;     /* free certificate name/value entries list */

/*
 * openSSL variables
 */

/* contexts */
static SSL_CTX* client_tls_ctx;
static SSL_CTX* client_dtls_ctx;
static SSL_CTX* server_tls_ctx;
static SSL_CTX* server_dtls_ctx;

/* server secret cookie */
unsigned char cookie_secret[COOKIE_SECRET_LENGTH];
/* cookie has been initialized */
int cookie_initialized;

/*******************************************************************************

Process network library error

Outputs an error message using the special syslib function, then halts.

*******************************************************************************/

static void netwrterr(const char* s)

{

    fprintf(stderr, "\nError: Network: %s\n", s);

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
        case enetopn:  netwrterr("Cannot reset or rewrite network file"); break;
        case enetpos:  netwrterr("Cannot position network file"); break;
        case enetloc:  netwrterr("Cannot find location network file"); break;
        case enetlen:  netwrterr("Cannot find length network file"); break;
        case esckeof:  netwrterr("end encountered on socket"); break;
        case efinuse:  netwrterr("File already in use"); break;
        case enetwrt:  netwrterr("Attempt to write to input side of network "
                                 "pair"); break;
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
        case enoext:   netwrterr("Could not get certificate extension"); break;
        case enomem:   netwrterr("Cannot allocate memory"); break;
        case ebufovf:  netwrterr("Buffer overflow"); break;
        case ecertpar: netwrterr("Error parsing certificate data"); break;
        case eunimp:   netwrterr("Feature not implemented"); break;
        case esystem:  netwrterr("System consistency check, please contact "
                                 "support"); break;

    }

}

/*******************************************************************************

Handle Linux error

Handles error in errno. Prints the string assocated with the error.

*******************************************************************************/

static void linuxerror(void)

{

    fprintf(stderr, "\nLinux Error: %s\n", strerror(errno));

    exit(1);

}
/*******************************************************************************

Handle SSL error queue

Dumps the SSL error queue. I don't know what the difference is between this and
SSL errors at this time.

*******************************************************************************/

static void sslerrorqueue(void)

{

    ERR_print_errors_fp(stderr);
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

    exit(1);

}

/*******************************************************************************

Get file entry

Gets a file entry, either from the free stack or by allocation. Clears the
fields in the file structure.

Note that we assume the malloc() calls kernel functions, and so we should not
call it with a lock.

*******************************************************************************/

static filptr getfil(void)

{

    filptr fp;

    if (frefil) { /* get existing free entry */

        pthread_mutex_lock(&fflock); /* take free file lock */
        fp = frefil; /* index top */
        frefil = fp->next; /* gap */
        pthread_mutex_unlock(&fflock); /* release */

    } else { /* allocate new one */

        /* get entry */
        fp = malloc(sizeof(filrec));
        if (!fp) error(enoallc); /* didn't work */
        /* allocate a lock for that (this is permanent) */
        pthread_mutex_init(&fp->lock, NULL);

    }
    fp->net = FALSE; /* set not network file */
    fp->sec = FALSE; /* set ordinary socket */
    fp->ssl = NULL;  /* clear SSL data */
    fp->cert = NULL; /* clear certificate data */
    fp->sfn = -1;    /* set no shadow */
    fp->opn = FALSE; /* set not open */
    fp->msg = FALSE; /* set not message port */
    fp->next = NULL; /* set no next */

    return (fp);

}

/*******************************************************************************

Put file entry

Places the given file entry on the free list, using the free list lock. This
routine is callable with a (unrelated) lock by the rules of structured locking,
but note it's sister function getfil is not lock callable.

*******************************************************************************/

static void putfil(filptr fp)

{

    pthread_mutex_lock(&fflock); /* take free file lock */
    fp->next = frefil; /* place on free list */
    frefil = fp;
    pthread_mutex_unlock(&fflock); /* release */

}

/*******************************************************************************

Make file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated.

*******************************************************************************/

static void makfil(int fn)

{

    filptr fp;

    /* See if the file entry is undefined, then get a file entry if not.
       This could be lapped by another thread, but we are saving the effort
       of taking a lock on each file entry validation. This is why we use
       a free list, even though we don't dispose of entries in the files
       array, just so we can have a supply of ready to use file entries. */
    if (!opnfil[fn]) {

        fp = getfil();
        pthread_mutex_lock(&oflock); /* take the table lock */
        if (!opnfil[fn]) opnfil[fn] = fp; /* still undefined, place */
        else putfil(fp); /* otherwise we got lapped, save the entry */
        pthread_mutex_unlock(&oflock); /* release the table lock */

    }

}

/*******************************************************************************

Get new file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated. Then the file entry is initialized.

*******************************************************************************/

static void newfil(int fn)

{

    filptr fp;

    /* See if the file entry is undefined, then get a file entry if not.
       This could be lapped by another thread, but we are saving the effort
       of taking a lock on each file entry validation. This is why we use
       a free list, even though we don't dispose of entries in the files
       array, just so we can have a supply of ready to use file entries. */
    if (!opnfil[fn]) {

        fp = getfil();
        pthread_mutex_lock(&oflock); /* take the table lock */
        if (!opnfil[fn]) opnfil[fn] = fp; /* still undefined, place */
        else putfil(fp); /* otherwise we got lapped, save the entry */
        pthread_mutex_unlock(&oflock); /* release the table lock */

    }
    pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
    opnfil[fn]->net = FALSE;  /* set unoccupied */
    opnfil[fn]->sec = FALSE;  /* set ordinary socket */
    opnfil[fn]->ssl = NULL;   /* clear SSL data */
    opnfil[fn]->cert = NULL;  /* clear certificate data */
    opnfil[fn]->sfn = -1;     /* set no shadow */
    opnfil[fn]->opn = FALSE;  /* set not open */
    opnfil[fn]->msg = FALSE;  /* set not message port */
    opnfil[fn]->sudp = FALSE; /* set not secure udp */
    pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

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
    union {
        struct sockaddr_storage ss;
        struct sockaddr_in6 s6;
        struct sockaddr_in s4;
    } peer;

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
    length += sizeof(in_port_t);
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
                   sizeof(in_port_t));
            memcpy(buffer + sizeof(peer.s4.sin_port),
                   &peer.s4.sin_addr,
                   sizeof(struct in_addr));
            break;
        case AF_INET6:
            memcpy(buffer,
                   &peer.s6.sin6_port,
                   sizeof(in_port_t));
            memcpy(buffer + sizeof(in_port_t),
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
    union {
        struct sockaddr_storage ss;
        struct sockaddr_in6 s6;
        struct sockaddr_in s4;
    } peer;

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
    length += sizeof(in_port_t);
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
                   sizeof(in_port_t));
            memcpy(buffer + sizeof(in_port_t),
                   &peer.s4.sin_addr,
                   sizeof(struct in_addr));
            break;
        case AF_INET6:
            memcpy(buffer,
                   &peer.s6.sin6_port,
                   sizeof(in_port_t));
            memcpy(buffer + sizeof(in_port_t),
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

Get server address v4

Retrieves a v4 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnet(string name, unsigned long* addr)

{

    struct addrinfo *p;
    int r;
    int af;

    af = FALSE; /* set address not found */
    r = getaddrinfo(name, NULL, NULL, &p);
    if (r) netwrterr(gai_strerror(r));
    while (p) {

        /* traverse the available addresses */
        if (p->ai_family == AF_INET && p->ai_socktype == SOCK_STREAM) {

            /* get the IPv4 address */
            *addr =
                ntohl(((struct sockaddr_in*)(p->ai_addr))->sin_addr.s_addr);
            af = TRUE; /* set an address found */

        }
        p = p->ai_next;

    }
    if (!af) error(enetadr); /* no address found */

}

/*******************************************************************************

Get 128 bit v6 address to 64 bit high/low

Gets a 128 bit v6 address from a high/low 64 bit address value.
Accommodates the difference between Linux and Mach/BSD structures.

*******************************************************************************/

void get128t64(struct sockaddr_in6* sap, unsigned long long* addrh,
              unsigned long long* addrl)

{

#ifndef __MACH__ /* Mac OS X */
    *addrh = (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[0]) << 32 |
            (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[1]);
    *addrl = (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[2]) << 32 |
            (unsigned long long) ntohl(sap->sin6_addr.__in6_u.__u6_addr32[3]);
#else
    *addrh = (unsigned long long) ntohl(sap->sin6_addr.__u6_addr.__u6_addr32[0]) << 32 |
            (unsigned long long) ntohl(sap->sin6_addr.__u6_addr.__u6_addr32[1]);
    *addrl = (unsigned long long) ntohl(sap->sin6_addr.__u6_addr.__u6_addr32[2]) << 32 |
            (unsigned long long) ntohl(sap->sin6_addr.__u6_addr.__u6_addr32[3]);
#endif

}

/*******************************************************************************

Get 64 bit high/low v6 address to 128 bit

Gets a 128 bit v6 address from a 64 bit high/low address.
Accommodates the difference between Linux and Mach/BSD structures.

*******************************************************************************/

void get64t128(unsigned long long addrh, unsigned long long addrl,
               struct sockaddr_in6* sap)

{

#ifndef __MACH__ /* Mac OS X */
    sap->sin6_addr.__in6_u.__u6_addr32[0] =
        (uint32_t) htonl(addrh >> 32 & 0xffffffff);
    sap->sin6_addr.__in6_u.__u6_addr32[1] =
        (uint32_t) htonl(addrh & 0xffffffff);
    sap->sin6_addr.__in6_u.__u6_addr32[2] =
        (uint32_t) htonl(addrl >> 32 & 0xffffffff);
    sap->sin6_addr.__in6_u.__u6_addr32[3] =
        (uint32_t) htonl(addrl & 0xffffffff);
#else
    sap->sin6_addr.__u6_addr.__u6_addr32[0] =
        (uint32_t) htonl(addrh >> 32 & 0xffffffff);
    sap->sin6_addr.__u6_addr.__u6_addr32[1] =
        (uint32_t) htonl(addrh & 0xffffffff);
    sap->sin6_addr.__u6_addr.__u6_addr32[2] =
        (uint32_t) htonl(addrl >> 32 & 0xffffffff);
    sap->sin6_addr.__u6_addr.__u6_addr32[3] =
        (uint32_t) htonl(addrl & 0xffffffff);
#endif

}

/*******************************************************************************

Get server address v6

Retrieves a v6 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnetv6(string name, unsigned long long* addrh,
                unsigned long long* addrl)

{

    struct addrinfo *p;
    int r;
    int af;
    struct sockaddr_in6* sap;

    af = FALSE; /* set address not found */
    r = getaddrinfo(name, NULL, NULL, &p);
    if (r) netwrterr(gai_strerror(r));
    while (p) {

        /* traverse the available addresses */
        if (p->ai_family == AF_INET6 && p->ai_socktype == SOCK_STREAM) {

            /* get the IPv6 address */
            sap = (struct sockaddr_in6*)(p->ai_addr);
            get128t64(sap, addrh, addrl); /* convert 128 bit to high/low 64 */
            af = TRUE; /* set an address found */

        }
        p = p->ai_next;

    }
    if (!af) error(enetadr); /* no address found */

}

/*******************************************************************************

Open network file

Opens a network file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either
TCP/IP or TCP/IP with a security layer, typically TLS 1.3 at this writing.

There are two versions of this routine, depending on if ipv4 or ipv6 addresses
are used.

*******************************************************************************/

static FILE* opennet(
    /* link is secured */     int secure,
    /* file open as socket */ int fn
)

{

    int   r;
    FILE* fp;
    int   sfn;
    SSL*  ssl;
    X509* cert;

    /* connect fid to FILE pointer for glibc use */
    fp = fdopen(fn, "r+");
    if (!fp) linuxerror();
    newfil(fn); /* get/renew file entry */
    pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
    opnfil[fn]->net = TRUE; /* set network (sockets) file */
    opnfil[fn]->sec = FALSE; /* set not secure */
    opnfil[fn]->opn = TRUE;
    pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    /* check secure sockets layer, and negotiate if so */
    if (secure) {

        /* to keep ssl handling from looping, we create a shadow fid so we
           can talk to the underlying socket */
        sfn = dup(fn); /* create second side for ssl operations */
        if (sfn == -1) error(enodupf);
        if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */
        newfil(sfn); /* init the shadow */

        pthread_mutex_lock(&opnfil[sfn]->lock); /* take file entry lock */
        opnfil[sfn]->net = TRUE; /* set network file */
        opnfil[sfn]->opn = TRUE;
        pthread_mutex_unlock(&opnfil[sfn]->lock); /* release file entry lock */

        ssl = SSL_new(client_tls_ctx); /* create new ssl */
        if (!ssl) error(esslnew);
        /* connect the ssl side to the shadow fid */
        r = SSL_set_fd(ssl, sfn); /* connect to fid */
        if (!r) error(esslfid);

        /* initiate tls handshake */
        r = SSL_connect(ssl);
        if (r != 1) sslerror(ssl, r);

        /* Get the remote certificate into the X509 structure.
           Right now we don't do anything with this (don't verify it) */
        cert = SSL_get_peer_certificate(ssl);
        if (!cert) error(esslcer);

        /* Update to file entry. The semantics of socket() and dup() dictate
           that no other open takes the fid, but I prefer to update the file
           entry atomically. */
        pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
        opnfil[fn]->sfn = sfn;
        opnfil[fn]->cert = cert;
        opnfil[fn]->ssl = ssl;
        opnfil[fn]->sec = TRUE; /* turn TLS encode/decode on for ssl channel */
        pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    }

    return (fp);

}

FILE* ami_opennet(/* IP address */      unsigned long addr,
                 /* port */            int port,
                 /* link is secured */ int secure
)

{

    struct sockaddr_in saddr;
    int fn;
    int r;

    /* set up address */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);
    saddr.sin_port = htons(port);

    /* connect the socket */
    fn = socket(AF_INET, SOCK_STREAM, 0);
    if (fn < 0) linuxerror();
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    r = connect(fn, (struct sockaddr*)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();

    /* finish with general routine */
    return (opennet(secure, fn));

}

FILE* ami_opennetv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    struct sockaddr_in6 saddr;
    int fn;
    int r;

    /* set up address */
    memset(&saddr, 0, sizeof(struct sockaddr_in6));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);
    saddr.sin6_port = htons(port);

    /* connect the socket */
    fn = socket(AF_INET6, SOCK_STREAM, 0);
    if (fn < 0) linuxerror();
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    r = connect(fn, (struct sockaddr*)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();

    /* finish with general routine */
    return (opennet(secure, fn));

}

/*******************************************************************************

Open message file

Opens a message file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either UDP, or
DTLS, with fixed length messages.

*******************************************************************************/

int ami_openmsg(
    /* ip address */      unsigned long addr,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    struct sockaddr_in saddr;
    int fn;
    socket_struct laddr;
    int r;
    struct timeval timeout;

    /* set up address */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);
    saddr.sin_port = htons(port);

    /* connect the socket */
    fn = socket(AF_INET, SOCK_DGRAM, 0);
    if (fn < 0) linuxerror();
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    newfil(fn); /* get/renew file entry */
    pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
    opnfil[fn]->net = TRUE; /* set network (sockets) file */
    opnfil[fn]->sec = FALSE; /* set not secure */
    opnfil[fn]->opn = TRUE;
    opnfil[fn]->msg = TRUE; /* set message socket */

    /* set up server address */
    memset(&opnfil[fn]->saddr.s4, 0, sizeof(struct sockaddr_in));

    /* copy address information from caller */
    memcpy(&opnfil[fn]->saddr.s4, &saddr, sizeof(struct sockaddr_in));
    pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    opnfil[fn]->v6addr = FALSE; /* set v4 address */

    /* set up for DTLS operation if selected */
    if (secure) {

        /* clear local */
        memset((void *) &laddr, 0, sizeof(struct sockaddr_storage));
        laddr.s4.sin_family = AF_INET;
        laddr.s4.sin_port = htons(0);
        r = bind(fn, (const struct sockaddr *) &laddr, sizeof(struct sockaddr_in));
        if (r) linuxerror();

        /* create socket struct */
        opnfil[fn]->ssl = SSL_new(client_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        /* Create BIO, connect and set to already connected */
        opnfil[fn]->bio = BIO_new_dgram(fn, BIO_CLOSE);
        r = connect(fn, (struct sockaddr *) &opnfil[fn]->saddr, sizeof(struct sockaddr_in));
        if (r) linuxerror();

        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &opnfil[fn]->saddr.ss);

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);

        r = SSL_connect(opnfil[fn]->ssl);
        if (r <= 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

    }

    return (fn); /* return fid */

}

int ami_openmsgv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    struct sockaddr_in6 saddr;
    int fn;
    socket_struct laddr;
    int r;
    struct timeval timeout;

    /* set up address */
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);
    saddr.sin6_port = htons(port);

    /* connect the socket */
    fn = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fn < 0) linuxerror();
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    newfil(fn); /* get/renew file entry */
    pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
    opnfil[fn]->net = TRUE; /* set network (sockets) file */
    opnfil[fn]->sec = FALSE; /* set not secure */
    opnfil[fn]->opn = TRUE;
    opnfil[fn]->msg = TRUE; /* set message socket */

    /* set up server address */
    memset(&opnfil[fn]->saddr.s6, 0, sizeof(struct sockaddr_in6));

    /* copy address information from caller */
    memcpy(&opnfil[fn]->saddr.s6, &saddr, sizeof(struct sockaddr_in6));
    pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    opnfil[fn]->v6addr = TRUE; /* set v6 address */

    /* set up for DTLS operation if selected */
    if (secure) {

        /* clear local */
        memset((void *) &laddr, 0, sizeof(struct sockaddr_storage));
        laddr.s6.sin6_family = AF_INET6;
        laddr.s6.sin6_port = htons(0);
        r = bind(fn, (const struct sockaddr *) &laddr, sizeof(struct sockaddr_in6));
        if (r) linuxerror();

        /* create socket struct */
        opnfil[fn]->ssl = SSL_new(client_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        /* Create BIO, connect and set to already connected */
        opnfil[fn]->bio = BIO_new_dgram(fn, BIO_CLOSE);
        r = connect(fn, (struct sockaddr *) &opnfil[fn]->saddr, sizeof(struct sockaddr_in6));
        if (r) linuxerror();

        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &opnfil[fn]->saddr.ss);

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);

        r = SSL_connect(opnfil[fn]->ssl);
        if (r <= 0) sslerror(opnfil[fn]->ssl, r);

        /* Set and activate timeouts */
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        /* set secure udp */
        opnfil[fn]->sudp = TRUE;

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

int ami_waitmsg(/* port number to wait on */ int port,
               /* secure mode */            int secure
               )

{

    socket_struct caddr;
    int fn, fn2;
    int r;
    int opt;
    struct timeval timeout;
    const int on = 1, off = 0;

    /* connect the socket */
    fn = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fn < 0) linuxerror();
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    newfil(fn); /* clear the fid entry */

    /* set socket options, multiple servers on address and same port */
    opt = 1;
    r = setsockopt(fn, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (r < 0) linuxerror();

    /* clear server address */
    memset(&opnfil[fn]->saddr, 0, sizeof(socket_struct));

    /* set up address */
    opnfil[fn]->saddr.s6.sin6_family = AF_INET6;
    opnfil[fn]->saddr.s6.sin6_addr = in6addr_any;
    opnfil[fn]->saddr.s6.sin6_port = htons(port);

    opnfil[fn]->v6addr = TRUE; /* set v6 address */

    /* bind to socket */
    r = bind(fn, (struct sockaddr *)&opnfil[fn]->saddr.s6,
                 sizeof(struct sockaddr_in6));
    if (r < 0) linuxerror();

    opnfil[fn]->net = TRUE; /* set network (sockets) file */
    opnfil[fn]->msg = TRUE; /* set message socket */

    /* set up for DTLS operation if selected */
    if (secure) {

           memset(&caddr, 0, sizeof(socket_struct));

        /* Create BIO */
        opnfil[fn]->bio = BIO_new_dgram(fn, BIO_NOCLOSE);

        /* Set and activate timeouts */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        BIO_ctrl(opnfil[fn]->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

        opnfil[fn]->ssl = SSL_new(server_dtls_ctx);
        if (!opnfil[fn]->ssl) sslerrorqueue();

        SSL_set_bio(opnfil[fn]->ssl, opnfil[fn]->bio, opnfil[fn]->bio);
        SSL_set_options(opnfil[fn]->ssl, SSL_OP_COOKIE_EXCHANGE);

        while (DTLSv1_listen(opnfil[fn]->ssl, (BIO_ADDR *) &caddr) <= 0);

        fn2 = socket(AF_INET6, SOCK_DGRAM, 0);
        if (fn2 < 0) linuxerror();

        setsockopt(fn2, SOL_SOCKET, SO_REUSEADDR, (const void*) &on, (socklen_t) sizeof(on));
        r = bind(fn2, (const struct sockaddr *) &opnfil[fn]->saddr.s6, sizeof(struct sockaddr_in6));
        if (r) linuxerror();
        r = connect(fn2, (struct sockaddr *) &caddr.s6, sizeof(struct sockaddr_in6));
        if (r) linuxerror();

        /* Set new fd and set BIO to connected */
        BIO_set_fd(SSL_get_rbio(opnfil[fn]->ssl), fn2, BIO_NOCLOSE);
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

*******************************************************************************/

int ami_maxmsg(unsigned long addr)

{

    struct sockaddr_in saddr;
    int fn;
    int r;
    int mtu;
    int mtulen;

    mtulen = sizeof(mtu); /* set length of word */

    /* set up target address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(addr);

    /* create socket */
    fn = socket(AF_INET, SOCK_DGRAM, 0);
    if (fn < 0) linuxerror();

    /* connect to address */
    r = connect(fn, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();

    /* find mtu */
    r = getsockopt(fn, IPPROTO_IP, IP_MTU, &mtu, (socklen_t*)&mtulen);
    if (r < 0) linuxerror();

    close(fn);

    return (mtu); /* return mtu */

}

/*******************************************************************************

Get maximum message size v6

Returns the maximum size of messages in the system. This is typically 1500, or
the maximum size of a UDP message, but can be larger, a so called "jumbo packet"
at 64kb, or somewhere in between.

We return the MTU reported by the interface. For a reliable network, this is
the absolute packet size. For others, it will be the MTU of the interface, which
packet breakage is possible.

*******************************************************************************/

int ami_maxmsgv6(unsigned long long addrh, unsigned long long addrl)

{

    struct sockaddr_in6 saddr;
    int fn;
    int r;
    int mtu;
    int mtulen;

    mtulen = sizeof(mtu); /* set length of word */

    /* set up target address */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    get64t128(addrh, addrl, &saddr);

    /* create socket */
    fn = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fn < 0) linuxerror();

    /* connect to address */
    r = connect(fn, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();

    /* find mtu */
    r = getsockopt(fn, IPPROTO_IP, IP_MTU, &mtu, (socklen_t*)&mtulen);
    if (r < 0) linuxerror();

    close(fn);

    return (mtu); /* return mtu */

}

/*******************************************************************************

Write message to message file

Writes a message to the given message file. The message file must be open. Any
size (including 0) up to ami_maxmsg() is allowed.

*******************************************************************************/

void ami_wrmsg(int fn, void* msg, unsigned long len)

{

    ssize_t r;
    int sr;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn]->msg) error(enotmsg);

    if (opnfil[fn]->sudp) { /* secure udp */

        sr = SSL_write(opnfil[fn]->ssl, msg, len);
        if (sr <= 0) sslerror(opnfil[fn]->ssl, sr);

    } else {

        /* write the message to socket, "non-blocking" (really just means
           no message confirmation) */
        if (opnfil[fn]->v6addr)
            r = sendto(fn, msg, len, MSG_DONTWAIT,
                       (const struct sockaddr *) &opnfil[fn]->saddr.s6,
                       sizeof(struct sockaddr_in6));
        else
            r = sendto(fn, msg, len, MSG_DONTWAIT,
                       (const struct sockaddr *) &opnfil[fn]->saddr.s4,
                       sizeof(struct sockaddr_in));
        if (r < 0) linuxerror();

    }

}

/*******************************************************************************

Read message from message file

Reads a message from the message file, of any length up to and including the
specified buffer length. The actual length transferred is returned. The length
of the buffer should be equal to maxmsg to pass all possible messages, unless it
is known that a given message size will never be exceeded.

*******************************************************************************/

int ami_rdmsg(int fn, void* msg, unsigned long len)

{

    ssize_t r;
    socklen_t al;
    int sr;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn]->msg) error(enotmsg);

    if (opnfil[fn]->sudp) { /* secure udp */

        sr = SSL_read(opnfil[fn]->ssl, msg, len);
        if (sr <= 0) sslerror(opnfil[fn]->ssl, sr);
        r = sr; /* set length read */

    } else {

        /* write the message to socket, blocking (get full UDP message) */
        if (opnfil[fn]->v6addr) {

            al = sizeof(struct sockaddr_in6);
            r = recvfrom(fn, msg, len, MSG_WAITALL,
                         (struct sockaddr *) &opnfil[fn]->saddr.s6, &al);

        } else {

            al = sizeof(struct sockaddr_in);
            r = recvfrom(fn, msg, len, MSG_WAITALL,
                         (struct sockaddr *) &opnfil[fn]->saddr.s4, &al);

        }
        if (r < 0) linuxerror();

    }

    return (r); /* exit with length of message */

}

/*******************************************************************************

Close message file

Closes the given message file.

*******************************************************************************/

void ami_clsmsg(int fn)

{

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* check is a message file */
    if (!opnfil[fn]->msg) error(enotmsg);

    close(fn); /* close the socket */

    /* if DTLS, free the ssl struct */
    if (opnfil[fn]->sudp) SSL_free(opnfil[fn]->ssl);

}

/*******************************************************************************

Wait external network connection

Waits for an external socket connection on a given port address. If an external
client connects to that port, then a socket file is opened and the file number
returned. Note that any number of such connections can be active at one time.
The program can invoke multiple tasks to handle each connection. If another
program tries to take the same port, it is blocked.

*******************************************************************************/

FILE* ami_waitnet(/* port number to wait on */ int port,
                 /* secure mode */            int secure
                )

{

    struct sockaddr_in6 saddr;
    int sfn, fn;
    int r;
    FILE* fp;
    int opt;
    SSL*  ssl;

    /* connect the socket */
    sfn = socket(AF_INET6, SOCK_STREAM, 0);
    if (sfn < 0) linuxerror();
    if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */
    newfil(sfn); /* clear the fid entry */

    /* set socket options, multiple servers on address and same port */
    opt = 1;
    r = setsockopt(sfn, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt));
    if (r < 0) linuxerror();

    /* set up address */
    saddr.sin6_family = AF_INET6;
    saddr.sin6_addr = in6addr_any;
    saddr.sin6_port = htons(port);
    r = bind(sfn, (struct sockaddr *)&saddr, sizeof(saddr));
    if (r < 0) linuxerror();

    /* wait on port */
    r = listen(sfn, 3);
    if (r < 0) linuxerror();

    /* accept connection, discard peer address */
    fn = accept(sfn, NULL, NULL);
    if (r < 0) linuxerror();
    if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */

    /* discard server port */
    close(sfn);

    /* set up as FILE* accessable */
    fp = fdopen(fn, "r+");
    if (!fp) linuxerror();
    newfil(fn); /* initialize file entry */
    opnfil[fn]->net = TRUE; /* set network (sockets) file */

    /* check secure sockets layer, and negotiate if so */
    if (secure) {

        /* to keep ssl handling from looping, we create a shadow fid so we
           can talk to the underlying socket */
        sfn = dup(fn); /* create second side for ssl operations */
        if (sfn == -1) error(enodupf);
        if (sfn < 0 || sfn >= MAXFIL) error(einvhan); /* invalid file handle */
        newfil(sfn); /* init the shadow */

        pthread_mutex_lock(&opnfil[sfn]->lock); /* take file entry lock */
        opnfil[sfn]->net = TRUE; /* set network file */
        opnfil[sfn]->opn = TRUE;
        pthread_mutex_unlock(&opnfil[sfn]->lock); /* release file entry lock */

        ssl = SSL_new(server_tls_ctx); /* create new ssl */
        if (!ssl) error(esslnew);
        /* connect the ssl side to the shadow fid */
        r = SSL_set_fd(ssl, sfn); /* connect to fid */
        if (!r) error(esslfid);

        /* perform ssl server accept */
        r = SSL_accept(ssl);
        if (r <= 0) sslerrorqueue();

        /* Update to file entry. The semantics of socket() and dup() dictate
           that no other open takes the fid, but I prefer to update the file
           entry atomically. */
        pthread_mutex_lock(&opnfil[fn]->lock); /* take file entry lock */
        opnfil[fn]->sfn = sfn;
        opnfil[fn]->ssl = ssl;
        opnfil[fn]->sec = TRUE; /* turn TLS encode/decode on for ssl channel */
        pthread_mutex_unlock(&opnfil[fn]->lock); /* release file entry lock */

    }

    return (fp);

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

int ami_relymsg(unsigned long addr)

{

    return (addr == 0x7f000001);

}

int ami_relymsgv6(unsigned long long addrh, unsigned long long addrl)

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

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

int ami_certmsg(int fn, int which, string buff, int len)

{

    X509* cert;
    STACK_OF(X509)* certstk;
    BIO* cb;
    int r;

    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (which < 1) error(einvctn); /* invalid certificate number */

    makfil(fn); /* create file entry as required */
    if (!opnfil[fn]->sudp && !opnfil[fn]->sec) error(enotsec);

    /* get the certificate */
    cert = SSL_get_peer_certificate(opnfil[fn]->ssl);
    if (!cert) error(enocert);

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

        }
        /* if the certificate number is out of range, return nothing */
        if (which > sk_X509_num(certstk)) cert = NULL;
        else cert = sk_X509_value(certstk, which-1);

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
        r = BIO_read(cb, buff, len);
        if (r < 0) error(erdbio);
        if (!BIO_eof(cb)) error(ecerttl);

    }

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

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

int ami_certnet(FILE* f, int which, string buff, int len)

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

/* print contents of bio */

static void prtbio(BIO *bp)

{

    char buff[4096];
    int r;

    do {

        r = BIO_gets(bp, buff, 4096);
        if (r > 0) printf("%.*s", r, buff);

    } while (r > 0);

}

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

*/

static void getnamval(char* buff, ami_certptr* list, int ident)

{

    char lbuff[CVBUFSIZ]; /* line output buffer */
    char vbuff[CVBUFSIZ]; /* value output buffer */
    char name[1024]; /* name */
    char* cp; /* output buffer pointer */
    ami_certptr cdp; /* current name/val being worked on */
    int l;

dbg_printf(dlinfo, "getnamval: buff:\n%s\n", buff);
    cdp = NULL; /* clear list entry */
    while (*buff) { /* loop over all lines in buffer */

        cp = lbuff; /* index output buffer */
        getlin(&buff, &cp); /* next next line */
        /* try to match a key */
        fndkey(lbuff, name, &cp);
        if (name[0]) {

dbg_printf(dlinfo, "getnamval: key found: %s\n", name);
            /* terminate any outstanding entry */
            remeol(vbuff); /* remove last \n, if exists */
            if (cdp) filldata(cdp, vbuff); /* place gathered data as value */
            cdp = addend(list, name); /* create n/v entry */
            vbuff[0] = 0; /* terminate empty value */
            if (*cp && *cp != '\n')
                /* there is more on the line, add to value */
                strcat(vbuff, cp); /* concatenate value */

        } else { /* no key, concatenate to value */

            /* does not begin with key, but there is no working key, then
               something is wrong. */
            if (!cdp) error(ecertpar);
            strcat(vbuff, cp); /* concatenate to value */

        }

    }
    /* terminate any outstanding entry */
    remeol(vbuff); /* remove last \n, if exists */
    if (cdp) filldata(cdp, vbuff); /* place gathered data as value */

}

void ami_certlistnet(FILE *f, int which, ami_certptr* list)

{

    X509* cert;
    STACK_OF(X509)* certstk;
    BIO* bp;
    int r;
    int fn;
    char* cp;
    char* cp2;
    int v;
    ASN1_INTEGER* sn;
    BIGNUM* bn;
    int i, j, l;
    ASN1_TIME* atp;
    char buff[CVBUFSIZ];
    char buff2[CVBUFSIZ];
    X509_EXTENSION* ep;
    ASN1_OBJECT* op;
    ASN1_OCTET_STRING *sp;
    BUF_MEM *bmp;
    unsigned nid;
    X509_PUBKEY* kp;
    EVP_PKEY* ekp;
    const X509_ALGOR *sig_alg;
    const ASN1_BIT_STRING *sig;
    /* branch placeholders */
    ami_certptr certificate;
    ami_certptr data;
    ami_certptr sigal;
    ami_certptr validity;
    ami_certptr extensions;
    ami_certptr cdp, cdp2;

    error(eunimp);

    fn = fileno(f); /* get fid */
    if (fn < 0 || fn >= MAXFIL) error(einvhan); /* invalid file handle */
    if (which < 1) error(einvctn); /* invalid certificate number */

    makfil(fn); /* create file entry as required */
    if (!opnfil[fn]->sudp && !opnfil[fn]->sec) error(enotsec);

    /* get the certificate */
    cert = SSL_get_peer_certificate(opnfil[fn]->ssl);
    if (!cert) error(enocert);

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

        }
        /* if the certificate number is out of range, return nothing */
        if (which > sk_X509_num(certstk)) cert = NULL;
        else cert = sk_X509_value(certstk, which-1);

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
        getnamval(buff, &cdp->fork, 0); /* parse n/v tree */

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
            getnamval(buff, &cdp->fork, 0); /* parse n/v tree */

        }

#if 0
printf("Signature Algorithm <start>\n");
        X509_get0_signature(&sig, &sig_alg, cert);
        X509_signature_print(bp, sig_alg, sig);
//        getbio(bp, buff, CVBUFSIZ);
        /* the key contained in the signature is indented. If we remove it, the
           key will be conforming */
//        cp = buff;
//        while (isspace(*cp)) cp++;
//         = addend(&certificate->fork, "Data"); /* make data branch */
       prtbio(bp);
       printf("\n");
printf("Signature Algorithm before aux print\n");

        X509_aux_print(bp, cert, 0);
        prtbio(bp);
        printf("\n");
printf("Signature Algorithm <end>\n");
#endif

    }

}

/*******************************************************************************

Get message certificate data list

Retrieves a list of data fields from the given file by number. The file
must contain an open and active DTLS connection. The data list is a list of
name - data pairs, both strings. The list can also have branches or forks, which
make it able to contain complete trees. The certificate number is from 1 to N
where N is the maximum certificate in the chain. Certificate 1 is the
certificate for the server connected. Certificate N is the CA or Certificate
Authority's certificate, AKA the root certificate. If there is no certificate
by that number, the resulting list is NULL.

Certificates are normally retrieved in numerical order, that is, 1, 2, 3...N.
Thus the end of the certificate chain must be found by traversal.

Note that the list is allocated by this routine, and the caller is responsible
for freeing the list as necessary.

Note that this routine retrieves the peer certificate, or other end of the
line. Servers are required to provide certificates. Clients are not.

*******************************************************************************/

void ami_certlistmsg(int fn, int which, ami_certptr* list)

{

    error(eunimp);

}

/*******************************************************************************

System call interdiction handlers

The interdiction calls are the basic system calls used to implement stdio:

read
write
open
close
lseek

We use interdiction to filter standard I/O calls towards the terminal. The
0 (input) and 1 (output) files are interdicted. In ANSI terminal, we act as a
filter, so this does not change the user ability to redirect the file handles
elsewhere.

*******************************************************************************/

/*******************************************************************************

Open

Terminal is assumed to be opened when the system starts, and closed when it
shuts down. Thus we do nothing for this.

*******************************************************************************/

static int ivopen(popen_t opendc, const char* pathname, int flags, int perm)

{

    int r;

    /* open with passdown */
    r = (*opendc)(pathname, flags, perm);
    if (r >= 0) {

        if (r < 0 || r > MAXFIL) error(einvhan); /* invalid file handle */
        if (opnfil[r]) { /* if not tracked, don't touch it */

            makfil(r); /* create file entry as required */
            /* open to close arguments on opposing threads can leave the open
               indeterminate, but this is just a state issue. */
            opnfil[r]->opn = TRUE; /* set open */

        }

    }

    return (r);

}

static int iopen(const char* pathname, int flags, int perm)

{

    return ivopen(ofpopen, pathname, flags, perm);

}

static int iopen_nocancel(const char* pathname, int flags, int perm)

{

    return ivopen(ofpopen_nocancel, pathname, flags, perm);

}

/*******************************************************************************

Close

Does nothing but pass on.

*******************************************************************************/

static int ivclose(pclose_t closedc, int fd)

{

    filrec fr; /* shadow of file entries */
    int r;

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    if (opnfil[fd]) { /* if not tracked, don't touch it */

        makfil(fd); /* create file entry as required */
        pthread_mutex_lock(&opnfil[fd]->lock); /* acquire lock */
        /* copy entry out and clear original. This keeps us from traveling with
           the lock and means we own the file data as locals */
        memcpy(&fr, opnfil[fd], sizeof(filrec));
        opnfil[fd]->ssl = NULL;
        opnfil[fd]->cert = NULL;
        opnfil[fd]->sfn = -1;
        pthread_mutex_unlock(&opnfil[fd]->lock); /* release lock */
        if (fr.sec) {

            if (fr.ssl) SSL_free(fr.ssl); /* free the ssl */
            if (fr.cert) X509_free(fr.cert); /* free certificate */
            (*closedc)(fr.sfn); /* close the shadow as well */

        }

    }
    r = (*closedc)(fd); /* normal file and socket close */

    /* open to close arguments on opposing threads can leave the open
       indeterminate, but this is just a state issue. */
    if (opnfil[fd]) opnfil[fd]->opn = FALSE;

    return (r);

}

static int iclose(int fd)

{

    return ivclose(ofpclose, fd);

}

static int iclose_nocancel(int fd)

{

    return ivclose(ofpclose_nocancel, fd);

}

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

*******************************************************************************/

static ssize_t ivread(pread_t readdc, int fd, void* buff, size_t count)

{

    ssize_t r;

    if (fd < 0 || fd >= MAXFIL) error(einvhan); /* invalid file handle */

    if (opnfil[fd]) { /* if not tracked, don't touch it */

        makfil(fd); /* create file entry as required */
        if (opnfil[fd]->sec)
            /* perform ssl decoded read */
            r = SSL_read(opnfil[fd]->ssl, buff, count);
        else r = (*readdc)(fd, buff, count); /* standard file and socket read */

    } else r = (*readdc)(fd, buff, count); /* standard file and socket read */

    return (r);

}

static ssize_t iread(int fd, void* buff, size_t count)

{

    return ivread(ofpread, fd, buff, count);

}

static ssize_t iread_nocancel(int fd, void* buff, size_t count)

{

    return ivread(ofpread_nocancel, fd, buff, count);

}

/*******************************************************************************

Write

*******************************************************************************/

static ssize_t ivwrite(pwrite_t writedc, int fd, const void* buff, size_t count)

{

    ssize_t r;

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    if (opnfil[fd]) { /* if not tracked, don't touch it */

        makfil(fd); /* create file entry as required */
        if (opnfil[fd]->sec)
            /* perform ssl encoded write */
            r = SSL_write(opnfil[fd]->ssl, buff, count);
        else
            /* standard file and socket write */
            r = (*writedc)(fd, buff, count);

    } else
        /* standard file and socket write */
        r = (*writedc)(fd, buff, count);

    return (r);

}

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    return ivwrite(ofpwrite, fd, buff, count);

}

static ssize_t iwrite_nocancel(int fd, const void* buff, size_t count)

{

    return ivwrite(ofpwrite_nocancel, fd, buff, count);

}

/*******************************************************************************

Lseek

Lseek is never possible on a network, so this just passed on.

*******************************************************************************/

static off_t ivlseek(plseek_t lseekdc, int fd, off_t offset, int whence)

{

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */

    return (*lseekdc)(fd, offset, whence);

}

static off_t ilseek(int fd, off_t offset, int whence)

{

    return ivlseek(ofplseek, fd, offset, whence);

}

/*******************************************************************************

Initialize SSL context

*******************************************************************************/

void initctx(
    /* context */ SSL_CTX** ctx,
    /* communications method */ const SSL_METHOD *method,
    /* certificate file */ string cert,
    /* key file */ string key
)

{

    int r;

    /* create new client TLS SSL context */
    *ctx = SSL_CTX_new(method);
    if (!*ctx) error(esslctx);

    /* Set the client key and cert */
    r = SSL_CTX_use_certificate_file(*ctx, cert, SSL_FILETYPE_PEM);
    if (r <= 0) sslerrorqueue();

    r = SSL_CTX_use_PrivateKey_file(*ctx, key, SSL_FILETYPE_PEM);
    if (r <= 0) sslerrorqueue();

    r = SSL_CTX_check_private_key(*ctx);
    if (r != 1) sslerrorqueue();

}

/*******************************************************************************

Network startup

*******************************************************************************/

static void ami_init_network (void) __attribute__((constructor (103)));
static void ami_init_network()

{

    int fi;
    int r;

    frefil = NULL; /* clear free files list */
    /* clear open files table */
    for (fi = 0; fi < MAXFIL; fi++) opnfil[fi] = NULL;

    /* initialize the open files lock */
    pthread_mutex_init(&oflock, NULL);
    /* initialize the free files lock */
    pthread_mutex_init(&fflock, NULL);

    /* override system calls for basic I/O */
    ovr_read(iread, &ofpread);
    ovr_write(iwrite, &ofpwrite);
    ovr_open(iopen, &ofpopen);
    ovr_close(iclose, &ofpclose);
    ovr_lseek(ilseek, &ofplseek);
#ifdef NOCANCEL
    ovr_read_nocancel(iread_nocancel, &ofpread_nocancel);
    ovr_write_nocancel(iwrite_nocancel, &ofpwrite_nocancel);
    ovr_open_nocancel(iopen_nocancel, &ofpopen_nocancel);
    ovr_close_nocancel(iclose_nocancel, &ofpclose_nocancel);
#endif

    /* initialize SSL library and register algorithms */
    if(SSL_library_init() < 0) error(einissl);
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();

    /* create new client TLS SSL context */
    initctx(&client_tls_ctx, TLS_client_method(), "client_tls_cert.pem",
                                                  "client_tls_key.pem");

    /* create new client DTLS SSL context */
    initctx(&client_dtls_ctx, DTLS_client_method(), "client_dtls_cert.pem",
                                                    "client_dtls_key.pem");

    /* create new server TLS SSL context */
    initctx(&server_tls_ctx, TLS_server_method(), "server_tls_cert.pem",
                                                  "server_tls_key.pem");

    /* configure server context */
    SSL_CTX_set_ecdh_auto(server_tls_ctx, 1);

    /* create new server DTLS SSL context */
    initctx(&server_dtls_ctx, DTLS_server_method(), "server_dtls_cert.pem",
                                                    "server_dtls_key.pem");

    /* configure server context */
    SSL_CTX_set_ecdh_auto(server_dtls_ctx, 1);

    /* Client has to authenticate */
    SSL_CTX_set_verify(server_dtls_ctx,
                       SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE,
                       dtls_verify_callback);

    SSL_CTX_set_session_cache_mode(server_dtls_ctx, SSL_SESS_CACHE_OFF);
    SSL_CTX_set_cookie_generate_cb(server_dtls_ctx, generate_cookie);
    SSL_CTX_set_cookie_verify_cb(server_dtls_ctx, &verify_cookie);

    /* set cookie uninitialized */
    cookie_initialized = FALSE;

}

/*******************************************************************************

Network shutdown

*******************************************************************************/

static void ami_deinit_network (void) __attribute__((destructor (103)));
static void ami_deinit_network()

{

    int fi;

    /* holding copies of system vectors */
    pread_t cppread;
    pread_t cppread_nocancel;
    pwrite_t cppwrite;
    pwrite_t cppwrite_nocancel;
    popen_t cppopen;
    popen_t cppopen_nocancel;
    pclose_t cppclose;
    pclose_t cppclose_nocancel;
    plseek_t cpplseek;

    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);

    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_lseek(ofplseek, &cpplseek);
#ifdef NOCANCEL
    ovr_read_nocancel(ofpread_nocancel, &cppread_nocancel);
    ovr_write_nocancel(ofpwrite_nocancel, &cppwrite_nocancel);
    ovr_open_nocancel(ofpopen_nocancel, &cppopen_nocancel);
    ovr_close_nocancel(ofpclose_nocancel, &cppclose_nocancel);
#endif

    /* close out open files and release space */
    for (fi = 0; fi < MAXFIL; fi++) if (opnfil[fi]) {

        pthread_mutex_destroy(&opnfil[fi]->lock);
        if (opnfil[fi]->opn) close(fi);
        if (opnfil[fi]->ssl) SSL_free(opnfil[fi]->ssl);
        if (opnfil[fi]->cert) X509_free(opnfil[fi]->cert);
        free(opnfil[fi]);

    }
    /* free context structures */
    SSL_CTX_free(client_tls_ctx);
    SSL_CTX_free(client_dtls_ctx);
    SSL_CTX_free(server_tls_ctx);
    SSL_CTX_free(server_dtls_ctx);

    /* release the open files lock */
    pthread_mutex_destroy(&oflock);
    /* release the free files lock */
    pthread_mutex_destroy(&fflock);

}
