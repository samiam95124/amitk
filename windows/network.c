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
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* Copyright (C) 2019 - Scott A. Franco                                         *
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
*******************************************************************************/

#define WINVER 0x0A00
#define _WIN32_WINNT 0xA00

/* Windows/mingw definitions */
#include <sys/types.h>
#include <stdio.h>
#include <windows.h>

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

#define MAXFIL 100 /* maximum number of open files */

/* types of system vectors for override calls */

typedef ssize_t (*pread_t)(int, void*, size_t);
typedef ssize_t (*pwrite_t)(int, const void*, size_t);
typedef int     (*popen_t)(const char*, int, int);
typedef int     (*pclose_t)(int);
typedef int     (*punlink_t)(const char*);
typedef off_t   (*plseek_t)(int, off_t, int);

/* system override calls */

extern void ovr_read(pread_t nfp, pread_t* ofp);
extern void ovr_write(pwrite_t nfp, pwrite_t* ofp);
extern void ovr_open(popen_t nfp, popen_t* ofp);
extern void ovr_close(pclose_t nfp, pclose_t* ofp);
extern void ovr_unlink(punlink_t nfp, punlink_t* ofp);
extern void ovr_lseek(plseek_t nfp, plseek_t* ofp);

/* File tracking.
   Files can be passthrough to syslib, or can be associated with a window. If
   on a window, they can be output, or they can be input. In the case of
   input, the file has its own input queue, and will receive input from all
   windows that are attached to it. */
typedef struct {

    int                net;   /* it's a network file */
    int                sock;  /* handle to network socket */
    struct sockaddr_in socka; /* socket address structure */

} filrec, *filptr;

/* error codes */
typedef enum {

    ewskini, /* cannot initialize winsock */
    einvhan, /* invalid file handle */
    enetopn, /* cannot open network file */
    enetpos, /* cannot position network file */
    enetloc, /* cannot find location network file */
    enetlen, /* cannot find length network file */
    esckeof, /* End encountered on socket */
    efinuse, /* file already in use */
    enetwrt, /* Attempt to write to input side of network pair */
    enomem,  /* Out of memory */
    enoipv4, /* Cannot find IPV4 address */
    enotimp, /* function not implemented */
    esystem  /* System consistency check */

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
static punlink_t ofpunlink;
static plseek_t  ofplseek;

filptr  opnfil[MAXFIL]; /* open files table */

/* The double fault flag is set when exiting, so if we exit again, it
  is checked, then forces an immediate exit. This keeps faults from
  looping. */
int     dblflt; /* double fault flag */

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
        case enetopn:  netwrterr("Cannot reset or rewrite network file");
                       break;
        case enetpos:  netwrterr("Cannot position network file"); break;
        case enetloc:  netwrterr("Cannot find location network file"); break;
        case enetlen:  netwrterr("Cannot find length network file"); break;
        case esckeof:  netwrterr("End encountered on socket"); break;
        case efinuse:  netwrterr("File already in use"); break;
        case enetwrt:
            netwrterr("Attempt to write to input side of network pair"); break;
        case enomem:   netwrterr("Out of memory"); break;
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

static void prtadr(unsigned long addr)

{

    fprintf(stderr, "%d.%d.%d.%d", addr >> 24 & 0xff,
            addr >> 16 & 0xff, addr >> 8 & 0xff, addr & 0xff);

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
    fprintf(stderr, "\n*** Windows error: %s\n", lpMsgBuf);

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

    /* get entry */
    fp = malloc(sizeof(filrec));
    if (!fp) error(enomem); /* didn't work */
    fp->net = FALSE; /* set not network file */

    return (fp);

}

/*******************************************************************************

Get new file entry

Checks the indicated file table entry, and allocates a new one if none is
allocated. Then the file entry is initialized.

*******************************************************************************/

void newfil(int fn)

{

    filptr fp;

    /* See if the file entry is undefined, then get a file entry if not */
    if (!opnfil[fn]) opnfil[fn] = getfil();
    opnfil[fn]->net = FALSE;  /* set unoccupied */

}

/*******************************************************************************

Get server address v4

Retrieves a v4 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnet(const string name, unsigned long* addr)

{

    HOSTENT* hep; /* host entry struct */


    hep = gethostbyname(name); /* get data structure for host address */
    if (!hep) wskerr(); /* process Winsock error */
    /* check ipv4 address */
    if (hep->h_addrtype != AF_INET) error(enoipv4);
    /* get first address */
    *addr = htonl(*(u_long*)hep->h_addr_list[0]);

}

/*******************************************************************************

Get server address v6

Retrieves a v6 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnetv6(string name, unsigned long long* addrh,
                unsigned long long* addrl)

{

    error(enotimp);

}

/*******************************************************************************

Open network file

Opens a network file with the given address and port. The file access is
broken up into a read and write side, that work independently, can can be
spread among different tasks. This is done because the Pascal file paradigm
does not handle read/write on the same file well.

*******************************************************************************/

FILE* ami_opennet(/* IP address */      unsigned long addr,
                 /* port */            int port,
                 /* link is secured */ int secure
)

{

    FILE*  fp;  /* file pointer */
    int    fn;  /* file number */
    filptr fep; /* file tracking pointer */
    int    r;   /* return value */

    if (secure) error(enotimp);
    /* open file handle as null */
    fp = fopen("nul", "w");
    fn = fileno(fp); /* get logical file no. */
    newfil(fn); /* get/renew file entry */
    fep = opnfil[fn]; /* index that */
    fep->net = TRUE; /* set as network file */

    /* open socket as internet, stream */
    fep->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (fep->sock == INVALID_SOCKET) wskerr(); /* process Winsock error */
    fep->socka.sin_family = PF_INET;
    /* note, parameters specified in big endian */
    fep->socka.sin_port = htons(port);
    fep->socka.sin_addr.s_addr = htonl(addr);
    r = connect(fep->sock, (struct sockaddr*)&fep->socka, sizeof(struct sockaddr_in));
    if (r == SOCKET_ERROR) wskerr(); /* process Winsock error */

    return (fp); /* exit with file */

}

FILE* ami_opennetv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    error(enotimp);

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

    error(enotimp);

}

int ami_openmsgv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    error(enotimp);

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

    error(enotimp);

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

    error(enotimp);

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

    error(enotimp);

}

/*******************************************************************************

Write message to message file

Writes a message to the given message file. The message file must be open. Any
size (including 0) up to ami_maxmsg() is allowed.

*******************************************************************************/

void ami_wrmsg(int fn, void* msg, unsigned long len)

{

    error(enotimp);

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

    error(enotimp);

}

/*******************************************************************************

Close message file

Closes the given message file.

*******************************************************************************/

void ami_clsmsg(int fn)

{

    error(enotimp);

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

    error(enotimp);

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

    error(enotimp);

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

    error(enotimp);

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

void ami_certlistnet(FILE *f, int which, ami_certptr* list)

{

    error(enotimp);

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

    error(enotimp);

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

/* close and clear file entry */

void clsfil(filptr fr)

{

    fr->net   = FALSE; /* set not network file */
    fr->sock  = 0;     /* clear out socket entry */

}

int iclose(int fd)

{

    int r;   /* result */
    int b;

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd]) { /* if not tracked, don't touch it */

        if (opnfil[fd]->net) { /* it's a network file */

            r = closesocket(opnfil[fd]->sock); /* close socket */
            if (r) wskerr(); /* cannot close socket */

        }
        clsfil(opnfil[fd]); /* close entry */

    } r = (*ofpclose)(fd); /* normal file and socket close */

    return (r);

}

/*******************************************************************************

Read file

Reads a byte buffer from the input file. If the file is normal, we pass it on.
If the file is a network file, we process a read on the associated socket.

*******************************************************************************/

static ssize_t iread(int fd, void* buff, size_t count)

{

    int r; /* int result */

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd]) { /* if not tracked, don't touch it */

        if (opnfil[fd]->net) { /* process network file */

            r = recv(opnfil[fd]->sock, buff, count, 0); /* receive network data */
            if (!r) error(esckeof); /* flag eof error */
            if (r != count) wskerr(); /* flag read error */

        }

    } else r = (*ofpread)(fd, buff, count); /* standard file and socket read */

    return (r);

}

/*******************************************************************************

Write file

Writes a byte buffer to the output file. If the file is normal, we pass it on.
If the file is a network file, we process a write on the associated socket.

*******************************************************************************/

static ssize_t iwrite(int fd, const void* buff, size_t count)

{

    int r; /* int result */

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */
    if (opnfil[fd]) { /* if not tracked, don't touch it */

        if (opnfil[fd]->net) { /* process network file */

            /* transmit network data */
            r = send(opnfil[fd]->sock, buff, count, 0);
            if (r != count) wskerr(); /* flag write error */

        }

    } else
        /* standard file and socket write */
        r = (*ofpwrite)(fd, buff, count);

    return (r);

}

/*******************************************************************************

Lseek

Lseek is never possible on a network, so this just passed on.

*******************************************************************************/

static off_t ilseek(int fd, off_t offset, int whence)

{

    if (fd < 0 || fd > MAXFIL) error(einvhan); /* invalid file handle */

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

dbg_printf(dlinfo, "begin\n");
WSACleanup();
    /* shutdown the open connections here, because the winsock dll is already
       shut down before the deinit handler gets executed */
    if (!dblflt) { /* we haven't already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->net)
                closesocket(opnfil[fi]->sock); /* close socket */

    }
dbg_printf(dlinfo, "end\n");

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

    /* perform winsock startup */
    r = WSAStartup(0x0002, &wsd);
    if (r) wskerr(); /* can't initalize Winsock */
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
    punlink_t cppunlink;
    plseek_t cpplseek;

    int fi; /* index for file tables */

    if (!dblflt) { /* we haven't already exited */

        dblflt = TRUE; /* set we already exited */
        /* close all open files */
        for (fi = 0; fi < MAXFIL; fi++)
            if (opnfil[fi] && opnfil[fi]->net)
                closesocket(opnfil[fi]->sock); /* close socket */

    }
    /* swap old vectors for existing vectors */
    ovr_read(ofpread, &cppread);
    ovr_write(ofpwrite, &cppwrite);
    ovr_open(ofpopen, &cppopen);
    ovr_close(ofpclose, &cppclose);
    ovr_unlink(ofpunlink, &cppunlink);
    ovr_lseek(ofplseek, &cpplseek);
    /* if we don't see our own vector flag an error */
    if (cppread != iread || cppwrite != iwrite || cppopen != iopen ||
        cppclose != iclose /* || cppunlink != iunlink */ || cpplseek != ilseek)
        error(esystem);
        /* release control handler */
    SetConsoleCtrlHandler(NULL, FALSE);

}
