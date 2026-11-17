/** ****************************************************************************
 *
 * Network library interface C++ wrapper
 *
 * Wraps the calls in network with C++ conventions, the same way
 * cpp/terminal.cpp wraps the terminal calls. This brings:
 *
 * 1. The functions and types do not need an "ami_" prefix: the network
 * namespace does the isolation. The secure flag defaults off.
 *
 * 2. The net and msg objects hold connections. A net is one stream
 * connection, opened as a client -- by address, or by name with the
 * lookup made on the way -- or as a server with waitnet; it converts
 * to FILE* so the stdio calls read and write it directly. A msg is one
 * message channel, the same way. The destructor closes whatever is
 * still held, so a connection cannot leak past an early return.
 *
 * These hold handles, not hooks, so any number of them may exist: a
 * mail program holds one net for IMAP and another for SMTP.
 *
 * Please see the Petit Ami documentation for more information.
 *
 ******************************************************************************/

extern "C" {

#include <stdio.h>

#include <localdefs.h>
#include <network.h>

}

#include "network.hpp"

namespace network {

/* procedures and functions */
void  addrnet(const char* name, ami_ulong* addr)
    { ami_addrnet((char*)name, addr); }
void  addrnetv6(const char* name, unsigned long long* addrh,
                unsigned long long* addrl)
    { ami_addrnetv6((char*)name, addrh, addrl); }
FILE* opennet(ami_ulong addr, ami_long port, ami_long secure)
    { return ami_opennet(addr, port, secure); }
FILE* opennetv6(unsigned long long addrh, unsigned long long addrl,
                ami_long port, ami_long secure)
    { return ami_opennetv6(addrh, addrl, port, secure); }
FILE* opennetname(const char* name, ami_long port, ami_long secure)
    { return ami_opennetname((char*)name, port, secure); }
ami_long  maxmsg(ami_ulong addr, ami_long secure)
    { return ami_maxmsg(addr, secure); }
ami_long  maxmsgv6(unsigned long long addrh, unsigned long long addrl, ami_long secure)
    { return ami_maxmsgv6(addrh, addrl, secure); }
ami_long  relymsg(ami_ulong addr) { return ami_relymsg(addr); }
ami_long  relymsgv6(unsigned long long addrh, unsigned long long addrl)
    { return ami_relymsgv6(addrh, addrl); }
ami_long  openmsg(ami_ulong addr, ami_long port, ami_long secure)
    { return ami_openmsg(addr, port, secure); }
ami_long  openmsgv6(unsigned long long addrh, unsigned long long addrl,
                ami_long port, ami_long secure)
    { return ami_openmsgv6(addrh, addrl, port, secure); }
void  wrmsg(ami_long fn, void* msg, ami_ulong len) { ami_wrmsg(fn, msg, len); }
ami_long  rdmsg(ami_long fn, void* msg, ami_ulong len)
    { return ami_rdmsg(fn, msg, len); }
ami_long  rdymsg(ami_long fn, ami_long usec) { return ami_rdymsg(fn, usec); }
void  tmomsg(ami_long fn, ami_long usec) { ami_tmomsg(fn, usec); }
void  bufmsg(ami_long fn, ami_long len) { ami_bufmsg(fn, len); }
void  shutmsg(ami_long fn) { ami_shutmsg(fn); }
void  clsmsg(ami_long fn) { ami_clsmsg(fn); }
void  neterror(neterrhan_t handler) { ami_neterror(handler); }
FILE* waitnet(ami_long port, ami_long secure) { return ami_waitnet(port, secure); }
ami_long  waitmsg(ami_long port, ami_long secure) { return ami_waitmsg(port, secure); }
ami_long  certnet(FILE* f, ami_long which, char* cert, ami_long len)
    { return ami_certnet(f, which, cert, len); }
ami_long  certmsg(ami_long fn, ami_long which, char* cert, ami_long len)
    { return ami_certmsg(fn, which, cert, len); }
ami_long  certvalidnet(FILE* f, ami_long which)
    { return ami_certvalidnet(f, which); }
ami_long  certvalidmsg(ami_long fn, ami_long which)
    { return ami_certvalidmsg(fn, which); }
ami_long  certsigned(const char* cert, ami_long clen, const char* signer,
                     ami_long slen)
    { return ami_certsigned((char*)cert, clen, (char*)signer, slen); }
void  certlistnet(FILE* f, ami_long which, certptr* list)
    { ami_certlistnet(f, which, (ami_certptr*)list); }
void  certlistmsg(ami_long fn, ami_long which, certptr* list)
    { ami_certlistmsg(fn, which, (ami_certptr*)list); }
void  certlistfree(certptr* list) { ami_certlistfree((ami_certptr*)list); }

/* methods */
net::net(void)

{

    f = NULL; /* nothing held yet */

}

net::~net(void)

{

    /* close what the object still holds */
    if (f) fclose(f);

}

/* An object holds one connection: opening over a held one lets go of
   the old one first. The same rule as the sound port objects. */
void net::opennet(ami_ulong addr, ami_long port, ami_long secure)

{

    if (f) fclose(f);
    f = ami_opennet(addr, port, secure);

}

/* the name form resolves on the way, and a secure connection learns the
   name to send the server */
void net::opennet(const char* name, ami_long port, ami_long secure)

{

    if (f) fclose(f);
    f = ami_opennetname((char*)name, port, secure);

}

void net::opennetv6(unsigned long long addrh, unsigned long long addrl,
                    ami_long port, ami_long secure)

{

    if (f) fclose(f);
    f = ami_opennetv6(addrh, addrl, port, secure);

}

void net::opennetv6(const char* name, ami_long port, ami_long secure)

{

    unsigned long long addrh, addrl;

    ami_addrnetv6((char*)name, &addrh, &addrl);
    opennetv6(addrh, addrl, port, secure);

}

void net::waitnet(ami_long port, ami_long secure)

{

    if (f) fclose(f);
    f = ami_waitnet(port, secure);

}

void net::close(void)

{

    if (f) { fclose(f); f = NULL; }

}

ami_long net::certnet(ami_long which, char* cert, ami_long len)
    { return ami_certnet(f, which, cert, len); }
ami_long net::certvalidnet(ami_long which)
    { return ami_certvalidnet(f, which); }
void net::certlistnet(ami_long which, certptr* list)
    { ami_certlistnet(f, which, (ami_certptr*)list); }

net::operator FILE*(void) { return f; }

msg::msg(void)

{

    fn = -1; /* nothing held yet */
    open = 0;

}

msg::~msg(void)

{

    /* close what the object still holds */
    if (open) ami_clsmsg(fn);

}

void msg::openmsg(ami_ulong addr, ami_long port, ami_long secure)

{

    if (open) ami_clsmsg(fn);
    fn = ami_openmsg(addr, port, secure);
    open = 1;

}

void msg::openmsg(const char* name, ami_long port, ami_long secure)

{

    ami_ulong addr;

    ami_addrnet((char*)name, &addr);
    openmsg(addr, port, secure);

}

void msg::openmsgv6(unsigned long long addrh, unsigned long long addrl,
                    ami_long port, ami_long secure)

{

    if (open) ami_clsmsg(fn);
    fn = ami_openmsgv6(addrh, addrl, port, secure);
    open = 1;

}

void msg::openmsgv6(const char* name, ami_long port, ami_long secure)

{

    unsigned long long addrh, addrl;

    ami_addrnetv6((char*)name, &addrh, &addrl);
    openmsgv6(addrh, addrl, port, secure);

}

void msg::waitmsg(ami_long port, ami_long secure)

{

    if (open) ami_clsmsg(fn);
    fn = ami_waitmsg(port, secure);
    open = 1;

}

void msg::wrmsg(void* buff, ami_ulong len) { ami_wrmsg(fn, buff, len); }
ami_long msg::rdmsg(void* buff, ami_ulong len)
    { return ami_rdmsg(fn, buff, len); }
ami_long msg::rdymsg(ami_long usec) { return ami_rdymsg(fn, usec); }
void msg::tmomsg(ami_long usec) { ami_tmomsg(fn, usec); }
void msg::bufmsg(ami_long len) { ami_bufmsg(fn, len); }
/* shut the channel under its reader; it is still held, and still closes */
void msg::shutmsg(void) { ami_shutmsg(fn); }

void msg::clsmsg(void)

{

    if (open) { ami_clsmsg(fn); open = 0; }

}

ami_long msg::certmsg(ami_long which, char* cert, ami_long len)
    { return ami_certmsg(fn, which, cert, len); }
ami_long msg::certvalidmsg(ami_long which)
    { return ami_certvalidmsg(fn, which); }
void msg::certlistmsg(ami_long which, certptr* list)
    { ami_certlistmsg(fn, which, (ami_certptr*)list); }

} /* namespace network */
