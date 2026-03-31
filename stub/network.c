/*******************************************************************************
*                                                                              *
*                                PETIT-AMI                                     *
*                                                                              *
*                             NETWORK MODULE                                   *
*                                                                              *
*                              Created 2020                                    *
*                                                                              *
*                              S. A. FRANCO                                    *
*                                                                              *
* This is a stubbed version. It exists so that Petit-Ami can be compiled and   *
* linked without all of the modules being complete.                            *
*                                                                              *
* All of the calls in this library print an error and exit.                    *
*                                                                              *
* One use of the stubs module is to serve as a prototype for a new module      *
* implementation.                                                              *
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

#include <stdlib.h>
#include <stdio.h>
#include <network.h>

/********************************************************************************

Process library error

Outputs an error message, then halts.

********************************************************************************/

static void error(char *s)
{

    fprintf(stderr, "\nError: Network: %s\n", s);

    exit(1);

}

/*******************************************************************************

Get server address v4

Retrieves a v4 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnet(string name, unsigned long* addr)

{

    error("ami_addrnet: Is not implemented");

}

/*******************************************************************************

Get server address v6

Retrieves a v6 server address by name. The name is given as a string. The
address is returned as an integer.

*******************************************************************************/

void ami_addrnetv6(string name, unsigned long long* addrh,
                unsigned long long* addrl)

{

    error("ami_addrnetv6: Is not implemented");

}

/*******************************************************************************

Open network file

Opens a network file with the given address, port and security. The file can be
both written and read. The protocol used for the transfer is either
TCP/IP or TCP/IP with a security layer, typically TLS 1.3 at this writing.

There are two versions of this routine, depending on if ipv4 or ipv6 addresses
are used.

*******************************************************************************/

FILE* ami_opennet(/* IP address */      unsigned long addr,
                 /* port */            int port,
                 /* link is secured */ int secure
)

{

    error("ami_opennet: Is not implemented");

    return (NULL); /* this just shuts up compiler */

}

FILE* ami_opennetv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    error("ami_opennetv6: Is not implemented");

    return (NULL); /* this just shuts up compiler */

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

    error("ami_openmsg: Is not implemented");

    return (1); /* this just shuts up compiler */

}

int ami_openmsgv6(
    /* v6 address low */  unsigned long long addrh,
    /* v6 address high */ unsigned long long addrl,
    /* port */            int port,
    /* link is secured */ int secure
)

{

    error("ami_openmsgv6: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("ami_waitmsg: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("ami_maxmsg: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("ami_maxmsgv6: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Write message to message file

Writes a message to the given message file. The message file must be open. Any
size (including 0) up to ami_maxmsg() is allowed.

*******************************************************************************/

void ami_wrmsg(int fn, void* msg, unsigned long len)

{

    error("ami_wrmsg: Is not implemented");

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

    error("ami_rdmsg: Is not implemented");

    return (1); /* this just shuts up compiler */

}

/*******************************************************************************

Close message file

Closes the given message file.

*******************************************************************************/

void ami_clsmsg(int fn)

{

    error("ami_clsmsg: Is not implemented");

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

    error("ami_waitnet: Is not implemented");

    return (NULL); /* this just shuts up compiler */

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

    error("ami_relymsg: Is not implemented");

    return (1); /* this just shuts up compiler */

}

int ami_relymsgv6(unsigned long long addrh, unsigned long long addrl)

{

    error("ami_relymsgv6: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("ami_certmsg: Is not implemented");

    return (1); /* this just shuts up compiler */

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

    error("ami_certnet: Is not implemented");

    return (1); /* this just shuts up compiler */

}

void ami_certlistnet(FILE *f, int which, ami_certptr* list)

{

    error("ami_certlistnet: Is not implemented");

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

    error("ami_certlistmsg: Is not implemented");

}
