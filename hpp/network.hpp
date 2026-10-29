/** ****************************************************************************
 *
 * Network library interface C++ wrapper declarations
 *
 * See cpp/network.cpp for the rationale. The certificate field record
 * appears here with the ami_ prefix stripped, laid out identically to
 * its C form: the wrapper casts between them.
 *
 ******************************************************************************/

#ifndef __NETWORK_HPP__
#define __NETWORK_HPP__

extern "C" {

#include <stdio.h>
#include <localdefs.h>

}

namespace network {

/* name - value pair list */
typedef struct certfield {

    char*             name;     /* name of field */
    char*             data;     /* content of field */
    ami_long          critical; /* is a critical X509 field */
    struct certfield* fork;     /* sublist */
    struct certfield* next;     /* next entry in list */

} certfield, *certptr;

/* procedures and functions */
void  addrnet(const char* name, ami_ulong* addr);
void  addrnetv6(const char* name, unsigned long long* addrh,
                unsigned long long* addrl);
FILE* opennet(ami_ulong addr, ami_long port, ami_long secure = 0);
FILE* opennetname(const char* name, ami_long port, ami_long secure = 0);
FILE* opennetv6(unsigned long long addrh, unsigned long long addrl,
                ami_long port, ami_long secure = 0);
ami_long  maxmsg(ami_ulong addr, ami_long secure = 0);
ami_long  maxmsgv6(unsigned long long addrh, unsigned long long addrl,
               ami_long secure = 0);
ami_long  relymsg(ami_ulong addr);
ami_long  relymsgv6(unsigned long long addrh, unsigned long long addrl);
ami_long  openmsg(ami_ulong addr, ami_long port, ami_long secure = 0);
ami_long  openmsgv6(unsigned long long addrh, unsigned long long addrl,
                ami_long port, ami_long secure = 0);
void  wrmsg(ami_long fn, void* msg, ami_ulong len);
ami_long  rdmsg(ami_long fn, void* msg, ami_ulong len);
void  clsmsg(ami_long fn);
FILE* waitnet(ami_long port, ami_long secure = 0);
ami_long  waitmsg(ami_long port, ami_long secure = 0);
ami_long  certnet(FILE* f, ami_long which, char* cert, ami_long len);
ami_long  certvalidnet(FILE* f, ami_long which);
ami_long  certmsg(ami_long fn, ami_long which, char* cert, ami_long len);
ami_long  certvalidmsg(ami_long fn, ami_long which);
ami_long  certsigned(const char* cert, ami_long clen, const char* signer,
                     ami_long slen);
void  certlistnet(FILE* f, ami_long which, certptr* list);
void  certlistmsg(ami_long fn, ami_long which, certptr* list);
void  certlistfree(certptr* list);

/*******************************************************************************

The net and msg objects

A net holds one stream connection, a msg one message channel. Opening
sets what the object holds -- as a client by address or, for a net, by
name, with the lookup made on the way -- or as a server with the wait
call. Every call after that leaves the connection off, and the
destructor closes whatever is still held, so a connection cannot leak
past an early return.

A net converts to FILE* wherever one is wanted, so the stdio calls
read and write it directly: fgets(buff, len, n) reads from the
connection n holds. Close it with the object -- close() or the
destructor -- never with fclose through the converted pointer, or the
destructor would close it a second time.

These hold handles, not hooks: any number of them may exist.

*******************************************************************************/

class net {

FILE* f; /* the connection held, NULL when closed */

public:

/* constructor */
net();

/* destructor */
~net();

/* copying is refused: two objects would free one connection */
net(const net&) = delete;
net& operator=(const net&) = delete;

/* methods */
void opennet(ami_ulong addr, ami_long port, ami_long secure = 0);
void opennet(const char* name, ami_long port, ami_long secure = 0);
void opennetv6(unsigned long long addrh, unsigned long long addrl,
               ami_long port, ami_long secure = 0);
void opennetv6(const char* name, ami_long port, ami_long secure = 0);
void waitnet(ami_long port, ami_long secure = 0);
void close(void);
ami_long certnet(ami_long which, char* cert, ami_long len);
ami_long certvalidnet(ami_long which);
void certlistnet(ami_long which, certptr* list);

/* the connection, for the stdio calls */
operator FILE*(void);

}; /* class net */

class msg {

ami_long fn;   /* the channel held */
ami_long open; /* it is held */

public:

/* constructor */
msg();

/* destructor */
~msg();

/* copying is refused: two objects would free one channel */
msg(const msg&) = delete;
msg& operator=(const msg&) = delete;

/* methods */
void openmsg(ami_ulong addr, ami_long port, ami_long secure = 0);
void openmsg(const char* name, ami_long port, ami_long secure = 0);
void openmsgv6(unsigned long long addrh, unsigned long long addrl,
               ami_long port, ami_long secure = 0);
void openmsgv6(const char* name, ami_long port, ami_long secure = 0);
void waitmsg(ami_long port, ami_long secure = 0);
void wrmsg(void* buff, ami_ulong len);
ami_long rdmsg(void* buff, ami_ulong len);
void clsmsg(void);
ami_long certmsg(ami_long which, char* cert, ami_long len);
ami_long certvalidmsg(ami_long which);
void certlistmsg(ami_long which, certptr* list);

}; /* class msg */

} /* namespace network */

#endif /* __NETWORK_HPP__ */
