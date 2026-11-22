/*******************************************************************************
*                                                                              *
*                              NETWORK TEST                                    *
*                                                                              *
* Automated test for the network library. Replaces the manual procedure in    *
* network_test.txt: each of the manual tests (gettys/telnet, msgserver/       *
* msgclient, prtcertnet, plain and secure) is run automatically against a     *
* loopback server run on a thread of this program, so no external server and  *
* no second console is needed.                                                 *
*                                                                              *
* The tests:                                                                  *
*                                                                              *
* 1. addrnet: name lookup of localhost gives the loopback address.            *
* 2. TCP connection in the clear: server waitnet, client opennet, a line      *
*    exchanged in each direction (the gettys/telnet test automated).          *
* 3. TCP connection secured (TLS): the same exchange with secure on.          *
* 4. Messages in the clear: server waitmsg, client openmsg, a message         *
*    exchanged in each direction (the msgserver/msgclient test automated).    *
* 5. Messages secured (DTLS): the same exchange with secure on.               *
* 6. maxmsg/relymsg: sanity of the message limits for the loopback address.   *
* 7. certnet: the raw certificate of the secure server is retrievable and     *
*    non-empty (the prtcertnet test automated against our own server), the    *
*    decoded certificate list (certlistnet) parses with the certificate       *
*    level signature value present, and certlistfree clears the list. The     *
*    server presents a two deep chain (localhost leaf signed by the           *
*    committed "Petit Ami Test CA"), so chain traversal is covered:           *
*    certificate 2 is the CA, with its critical basic constraints flag        *
*    propagated, and past the end of the chain gives empty/NULL results       *
*    without error. The DTLS run covers the message side queries             *
*    (certmsg, certlistmsg) directly.                                         *
* 8. The IPv6 forms of all of the above: addrnetv6, opennetv6, openmsgv6,     *
*    maxmsgv6 and relymsgv6, against the same loopback servers, which bind    *
*    dual stack. The certificate queries are also made on a v6 connection.    *
* 9. Contract corners: the critical buffer clause of certnet at the exact     *
*    certificate length (fills, no terminator; one byte more terminates);     *
*    rdmsg with a buffer shorter than the message (datagram truncation);      *
*    relymsg says loopback is reliable, which it is by definition; two        *
*    connections open at once with their replies kept apart; and the          *
*    refused connection retry of opennet, run against a server that is       *
*    deliberately slow to listen instead of left to a startup race.          *
*                                                                              *
* Run from the petit-ami/amitk root so the TLS test certificates              *
* (client_tls_cert.pem and friends) are found in the current directory.      *
*                                                                              *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <localdefs.h>
#include <services.h>
#include <network.h>

#define BUFLEN 250

/* ports for the loopback servers; one per test to avoid reuse collisions */
#define PORT_TCP   24421
#define PORT_TLS   24422
#define PORT_MSG   24423
#define PORT_DTLS  24424
#define PORT_CERT  24425
#define PORT_TCP6  24426
#define PORT_TLS6  24427
#define PORT_MSG6  24428
#define PORT_DTLS6 24429
#define PORT_MAXM  24430
#define PORT_MAXM6 24431
#define PORT_TRUNC 24432
#define PORT_SIM1  24433
#define PORT_SIM2  24434
#define PORT_RETRY 24435
#define PORT_NAME  24436
#define PORT_NAMES 24437

static int passes = 0;
static int fails  = 0;

static void result(const char* name, int pass)

{

    printf("%-40s %s\n", name, pass ? "pass" : "*** FAIL ***");
    if (pass) passes++; else fails++;

}

/* print a section banner: what is under test */
static void section(const char* title)

{

    printf("\n----- %s -----\n\n", title);

}

/*******************************************************************************

Loopback servers

Each server runs on a thread started with the services thread api: it serves
exactly one client, then flags completion. The client side hands the server
its parameters in the statics below before starting it (the tests run one at
a time, so one set suffices), and synchronizes with it by lock and signal.

*******************************************************************************/

static ami_long lockid;   /* handshake lock */
static ami_long sigid;    /* handshake signal */

static int srvport;   /* port for the server to serve */
static int srvsecure; /* serve secured */
static int clidone;   /* client is finished with the connection */
static int srvdone;   /* server has finished */

/* Let a message server come up before sending to it. waitmsg binds and
   reads in a single call, so there is no ready event the server could flag
   before it blocks, and a datagram sent before the bind is silently lost -
   that is UDP, not a race the client can heal. The TCP tests need no such
   delay: opennet retries refused connections on a short backoff, which is
   part of what these tests exercise. */
static void waitsrv(void)

{

#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif

}

/* start a server thread on the given port */
static void startsrv(void (*srv)(void), int port, int secure)

{

    srvport = port;
    srvsecure = secure;
    clidone = FALSE;
    srvdone = FALSE;
    ami_newthread(srv);

}

/* tell the server we are done with the connection */
static void clientdone(void)

{

    ami_lock(lockid);
    clidone = TRUE;
    ami_sendsig(sigid);
    ami_unlock(lockid);

}

/* flag server completion (run on the server thread) */
static void serverdone(void)

{

    ami_lock(lockid);
    srvdone = TRUE;
    ami_sendsig(sigid);
    ami_unlock(lockid);

}

/* finish with a server thread */
static void finish(void)

{

    ami_lock(lockid);
    while (!srvdone) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);

}

/* TCP server: read a line from the client, send a line back */
static void tcpserver(void)

{

    FILE* f;
    char buff[BUFLEN];

    f = ami_waitnet(srvport, srvsecure);
    if (fgets(buff, BUFLEN, f))
        ; /* client line received */
    fputs("Hello, client\n", f);
    fflush(f);
    /* hold the connection up until the client has read and the certificate
       tests have inspected, then close it */
    ami_lock(lockid);
    while (!clidone) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);
    fclose(f);
    serverdone();

}

/* message server: receive one message, send one back */
static void msgserver(void)

{

    ami_long fn;
    ami_long len;
    char buff[BUFLEN];

    fn = ami_waitmsg(srvport, srvsecure);
    len = ami_rdmsg(fn, buff, BUFLEN);
    ami_wrmsg(fn, "Hello, client", 13);
    ami_clsmsg(fn);
    serverdone();

}

/* TCP server that is slow to come up: the client is already connecting
   while this sleeps, so its connects are refused until the listen -- which
   is the retry path in opennet, run deterministically instead of by the
   race the other tests leave to chance */
static void tcpslowserver(void)

{

    waitsrv();
    waitsrv();
    tcpserver();

}

/* A second TCP server with state of its own, so two can serve at once.
   The lock and signal are shared: a signal wakes every waiter and each
   loops on its own flag, so the pairs cannot take each other's events. */
static int srv2port;
static int cli2done;
static int srv2done;

static void tcpserver2(void)

{

    FILE* f;
    char buff[BUFLEN];

    f = ami_waitnet(srv2port, FALSE);
    if (fgets(buff, BUFLEN, f))
        ; /* client line received */
    fputs("Hello, client 2\n", f);
    fflush(f);
    ami_lock(lockid);
    while (!cli2done) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);
    fclose(f);
    ami_lock(lockid);
    srv2done = TRUE;
    ami_sendsig(sigid);
    ami_unlock(lockid);

}

/* message server: receive one large message, reply with its length. The
   buffers are static: maximum size messages are too large for a thread
   stack */
static char maxrecv[70000];

static void msgmaxserver(void)

{

    ami_long fn;
    ami_long len;
    char reply[BUFLEN];

    fn = ami_waitmsg(srvport, srvsecure);
    len = ami_rdmsg(fn, maxrecv, sizeof(maxrecv));
    sprintf(reply, "%lld", AMI_LONG_CAST(len));
    ami_wrmsg(fn, reply, strlen(reply));
    ami_clsmsg(fn);
    serverdone();

}

/*******************************************************************************

Tests

*******************************************************************************/

/* find a named entry in a certificate list tree, all levels */
static ami_certptr fndnode(ami_certptr list, const char* name)

{

    ami_certptr cp;
    ami_certptr fp;

    for (cp = list; cp; cp = cp->next) {

        if (cp->name && !strcmp(cp->name, name)) return (cp);
        if (cp->fork) {

            fp = fndnode(cp->fork, name);
            if (fp) return (fp);

        }

    }

    return (NULL);

}

/* test 1: name lookup */
static void taddrnet(void)

{

    ami_ulong addr;

    addr = 0;
    ami_addrnet("localhost", &addr);
    result("addrnet localhost (hosts file)", addr == 0x7f000001ul);
    printf("    localhost -> %llu.%llu.%llu.%llu\n", AMI_ULONG_CAST(addr>>24 & 0xff),
           AMI_ULONG_CAST(addr>>16 & 0xff), AMI_ULONG_CAST(addr>>8 & 0xff), AMI_ULONG_CAST(addr & 0xff));
    addr = 0;
    ami_addrnet("127.0.0.1", &addr);
    result("addrnet numeric literal", addr == 0x7f000001ul);
    printf("    \"127.0.0.1\" -> %llu.%llu.%llu.%llu\n", AMI_ULONG_CAST(addr>>24 & 0xff),
           AMI_ULONG_CAST(addr>>16 & 0xff), AMI_ULONG_CAST(addr>>8 & 0xff), AMI_ULONG_CAST(addr & 0xff));

}

/* test 1a: name lookup, IPv6 */
static void taddrnetv6(void)

{

    unsigned long long addrh, addrl;

    addrh = 1;
    addrl = 0;
    ami_addrnetv6("::1", &addrh, &addrl);
    result("addrnetv6 numeric loopback", addrh == 0 && addrl == 1);
    printf("    \"::1\" -> %llx:%llx (high:low 64 bit halves)\n", addrh, addrl);

}

/* Live DNS resolution, v4 and v6, against google.com (chosen as reliable).
   Note on a machine with no name service this errors out, which stops the
   program per the error model. The addresses themselves rotate; some
   non-zero, non-loopback answer is the check. */
static void taddrdns(void)

{

    ami_ulong addr;
    unsigned long long addrh, addrl;

    addr = 0;
    ami_addrnet("google.com", &addr);
    result("addrnet live DNS (google.com)",
           addr != 0 && addr != 0x7f000001ul);
    printf("    google.com A    -> %llu.%llu.%llu.%llu\n", AMI_ULONG_CAST(addr>>24 & 0xff),
           AMI_ULONG_CAST(addr>>16 & 0xff), AMI_ULONG_CAST(addr>>8 & 0xff), AMI_ULONG_CAST(addr & 0xff));
    addrh = 0;
    addrl = 0;
    ami_addrnetv6("google.com", &addrh, &addrl);
    result("addrnetv6 live DNS (google.com)",
           (addrh != 0 || addrl != 0) && !(addrh == 0 && addrl == 1));
    printf("    google.com AAAA -> %llx:%llx (high:low 64 bit halves)\n",
           addrh, addrl);

}

/* tests 2 and 3: TCP line exchange, clear or secured */
static void ttcp(const char* name, int port, int secure)

{

    ami_ulong addr;
    FILE* f;
    char  buff[BUFLEN];
    int   pass;

    pass = FALSE;
    startsrv(tcpserver, port, secure);
    ami_addrnet("localhost", &addr);
    f = ami_opennet(addr, port, secure);
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f))
        pass = !strcmp(buff, "Hello, client\n");
    fclose(f);
    clientdone();
    finish();
    result(name, pass);
    printf("    sent \"Hello, server\", received \"Hello, client\" over %s\n",
           secure ? "TLS" : "TCP in the clear");

}

/* The line exchange again, with the connection opened by name. The secured
   run also checks the chain's links: the loopback leaf is signed by the test
   CA, the test CA by itself, and there is no third certificate. Then the same
   links through certsigned on the certificate texts, which is how a chain is
   finished against a root the server did not send */
static void tname(const char* name, int port, int secure)

{

    FILE* f;
    char  buff[BUFLEN];
    char  leaf[10000], ca[10000];
    ami_long ll, cl;
    int   pass;

    pass = FALSE;
    startsrv(tcpserver, port, secure);
    f = ami_opennetname("localhost", port, secure);
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f))
        pass = !strcmp(buff, "Hello, client\n");
    if (secure) {

        result("certvalidnet leaf signed by the test CA", ami_certvalidnet(f, 1) == 1);
        result("certvalidnet test CA signs itself", ami_certvalidnet(f, 2) == 1);
        result("certvalidnet past chain end is 0", ami_certvalidnet(f, 3) == 0);
        ll = ami_certnet(f, 1, leaf, sizeof(leaf));
        cl = ami_certnet(f, 2, ca, sizeof(ca));
        result("certsigned leaf by test CA", ami_certsigned(leaf, ll, ca, cl) == 1);
        result("certsigned test CA by leaf is 0", ami_certsigned(ca, cl, leaf, ll) == 0);
        result("certsigned on non-certificate text is 0",
               ami_certsigned(buff, (ami_long)strlen(buff), ca, cl) == 0);

    }
    fclose(f);
    clientdone();
    finish();
    result(name, pass);
    printf("    sent \"Hello, server\", received \"Hello, client\" over %s, "
           "opened by name\n", secure ? "TLS" : "TCP in the clear");

}

/* IPv6 variant of the TCP line exchange, clear or secured */
static void ttcpv6(const char* name, int port, int secure)

{

    unsigned long long addrh, addrl;
    FILE* f;
    char  buff[BUFLEN];
    char  cert[10000];
    ami_long  r;
    ami_certptr list;
    ami_certptr cp;
    int   pass;

    pass = FALSE;
    startsrv(tcpserver, port, secure);
    ami_addrnetv6("::1", &addrh, &addrl);
    f = ami_opennetv6(addrh, addrl, port, secure);
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f))
        pass = !strcmp(buff, "Hello, client\n");
    if (secure) {

        /* the certificate queries on a v6 connection: the same server
           chain arrives whichever stack carried it */
        memset(cert, 0, sizeof(cert));
        r = ami_certnet(f, 1, cert, sizeof(cert));
        result("certnet raw certificate (IPv6)", strlen(cert) > 0);
        printf("    TLS server certificate PEM over IPv6: %lld bytes\n", AMI_LONG_CAST(r));
        list = NULL;
        ami_certlistnet(f, 1, &list);
        cp = fndnode(list, "Issuer");
        result("certlist leaf issuer is test CA (IPv6)", cp && cp->data &&
               strstr(cp->data, "Petit Ami Test CA") != NULL);
        ami_certlistfree(&list);

    }
    fclose(f);
    clientdone();
    finish();
    result(name, pass);
    printf("    sent \"Hello, server\", received \"Hello, client\" over %s\n",
           secure ? "TLS on IPv6" : "TCP in the clear on IPv6");

}

/* tests 4 and 5: message exchange, clear or secured. The secured (DTLS) run
   also covers the message side certificate queries: the raw certificate,
   the decoded list, and the list free, against our own DTLS server */
static void tmsg(const char* name, int port, int secure)

{

    ami_ulong addr;
    ami_long fn;
    ami_long len;
    char buff[BUFLEN];
    int  pass;
    char cert[10000];
    ami_long r;
    ami_certptr list;
    ami_certptr cp;

    pass = FALSE;
    startsrv(msgserver, port, secure);
    waitsrv(); /* messages have no connect retry, let the server come up */
    ami_addrnet("localhost", &addr);
    fn = ami_openmsg(addr, port, secure);
    ami_wrmsg(fn, "Hello, server", 13);
    len = ami_rdmsg(fn, buff, BUFLEN);
    if (secure) {

        /* certificate queries on the message connection; the peer
           certificate is cached with the handshake, so these are valid on
           the open message file */
        memset(cert, 0, sizeof(cert));
        r = ami_certmsg(fn, 1, cert, sizeof(cert));
        result("certmsg raw certificate (DTLS)", strlen(cert) > 0);
        printf("    DTLS server certificate PEM: %lld bytes\n", AMI_LONG_CAST(r));
        list = NULL;
        ami_certlistmsg(fn, 1, &list);
        cp = fndnode(list, "Signature Value");
        result("certlistmsg decodes (DTLS)", list && list->name &&
               !strcmp(list->name, "Certificate") &&
               cp && cp->data && cp->data[0]);
        cp = fndnode(list, "Subject");
        printf("    DTLS certificate subject: %s\n",
               cp && cp->data ? cp->data : "<none>");
        ami_certlistfree(&list);
        result("certlistfree clears list (DTLS)", list == NULL);

        /* the chain, as on the stream side: the DTLS server presents the
           same two deep chain, so certificate 2 is the test CA */
        memset(cert, 0, sizeof(cert));
        r = ami_certmsg(fn, 2, cert, sizeof(cert));
        result("certmsg chain CA raw (DTLS)", strlen(cert) > 0);
        list = NULL;
        ami_certlistmsg(fn, 2, &list);
        cp = fndnode(list, "Subject");
        result("certlistmsg chain CA decodes (DTLS)", cp && cp->data &&
               strstr(cp->data, "Petit Ami Test CA") != NULL);
        printf("    DTLS certificate 2 subject: %s\n",
               cp && cp->data ? cp->data : "<none>");
        ami_certlistfree(&list);

        /* and past its end: empty and NULL, without error */
        memset(cert, 0, sizeof(cert));
        r = ami_certmsg(fn, 3, cert, sizeof(cert));
        result("certmsg past chain end is empty (DTLS)", r == 0 && cert[0] == 0);
        list = NULL;
        ami_certlistmsg(fn, 3, &list);
        result("certlistmsg past chain end is NULL (DTLS)", list == NULL);

    }
    if (len == 13) pass = !strncmp(buff, "Hello, client", 13);
    ami_clsmsg(fn);
    finish();
    result(name, pass);
    printf("    sent 13 byte message, received %lld byte reply over %s\n",
           AMI_LONG_CAST(len), secure ? "DTLS" : "UDP in the clear");

}

/* IPv6 variant of the message exchange, clear or secured */
static void tmsgv6(const char* name, int port, int secure)

{

    unsigned long long addrh, addrl;
    ami_long fn;
    ami_long len;
    char buff[BUFLEN];
    int  pass;

    pass = FALSE;
    startsrv(msgserver, port, secure);
    waitsrv(); /* messages have no connect retry, let the server come up */
    ami_addrnetv6("::1", &addrh, &addrl);
    fn = ami_openmsgv6(addrh, addrl, port, secure);
    ami_wrmsg(fn, "Hello, server", 13);
    len = ami_rdmsg(fn, buff, BUFLEN);
    if (len == 13) pass = !strncmp(buff, "Hello, client", 13);
    ami_clsmsg(fn);
    finish();
    result(name, pass);
    printf("    sent 13 byte message, received %lld byte reply over %s\n",
           AMI_LONG_CAST(len), secure ? "DTLS on IPv6" : "UDP in the clear on IPv6");

}

/* test 6: message limits */
static void tmsglim(void)

{

    ami_ulong addr;
    ami_long max;
    ami_long rely;

    ami_addrnet("localhost", &addr);
    max = ami_maxmsg(addr, FALSE);
    result("maxmsg sane", max > 0);
    printf("    maximum message for loopback: %lld bytes\n", AMI_LONG_CAST(max));
    /* the secure limit answers to the DTLS record: one record per datagram,
       at most 16K of payload however large the route's packets run */
    max = ami_maxmsg(addr, TRUE);
    result("maxmsg secure honors the DTLS record", max > 0 && max <= 16384);
    printf("    maximum secure message for loopback: %lld bytes\n", AMI_LONG_CAST(max));
    rely = ami_relymsg(addr);
    result("relymsg loopback is reliable", rely == 1);
    printf("    loopback delivery reliable: %s\n", rely ? "yes" : "no");

}

/* A message of exactly maxmsg bytes is deliverable: send one through the
   loopback and have the server echo the received length. In the clear only
   here; asked with secure true, maxmsg answers for the DTLS record limit
   instead. The send buffer is static, like the server receive buffer */
static char maxsend[70000];

static void tmsgmax(const char* name, int port, int v6)

{

    ami_ulong addr;
    unsigned long long addrh, addrl;
    ami_long max;
    ami_long fn;
    ami_long len;
    char reply[BUFLEN];
    int  pass;

    pass = FALSE;
    startsrv(msgmaxserver, port, FALSE);
    waitsrv(); /* messages have no connect retry, let the server come up */
    if (v6) {

        ami_addrnetv6("::1", &addrh, &addrl);
        max = ami_maxmsgv6(addrh, addrl, FALSE);
        fn = ami_openmsgv6(addrh, addrl, port, FALSE);

    } else {

        ami_addrnet("localhost", &addr);
        max = ami_maxmsg(addr, FALSE);
        fn = ami_openmsg(addr, port, FALSE);

    }
    if (max > (ami_long)sizeof(maxsend)) max = sizeof(maxsend);
    ami_wrmsg(fn, maxsend, max);
    len = ami_rdmsg(fn, reply, BUFLEN-1);
    if (len >= 0) {

        reply[len] = 0;
        pass = atol(reply) == max;

    }
    ami_clsmsg(fn);
    finish();
    result(name, pass);
    printf("    sent %lld bytes (the maxmsg%s limit), server received %s\n",
           AMI_LONG_CAST(max), v6 ? "v6" : "", len >= 0 ? reply : "<none>");

}

/* IPv6 variant of the message limit tests */
static void tmsglimv6(void)

{

    unsigned long long addrh, addrl;
    ami_long max;
    ami_long rely;

    ami_addrnetv6("::1", &addrh, &addrl);
    max = ami_maxmsgv6(addrh, addrl, FALSE);
    result("maxmsgv6 sane", max > 0);
    printf("    maximum message for v6 loopback: %lld bytes\n", AMI_LONG_CAST(max));
    /* the secure limit answers to the DTLS record: one record per datagram,
       at most 16K of payload however large the route's packets run */
    max = ami_maxmsgv6(addrh, addrl, TRUE);
    result("maxmsgv6 secure honors the DTLS record", max > 0 && max <= 16384);
    printf("    maximum secure message for v6 loopback: %lld bytes\n", AMI_LONG_CAST(max));
    rely = ami_relymsgv6(addrh, addrl);
    result("relymsgv6 loopback is reliable", rely == 1);
    printf("    v6 loopback delivery reliable: %s\n", rely ? "yes" : "no");

}

/* A message longer than the buffer given to rdmsg: datagram semantics,
   the buffer's worth arrives and the rest of the message is gone. The
   reply is thirteen bytes read into five. */
static void trdtrunc(void)

{

    ami_ulong addr;
    ami_long fn;
    ami_long len;
    char five[5];
    int  pass;

    startsrv(msgserver, PORT_TRUNC, FALSE);
    waitsrv();
    ami_addrnet("localhost", &addr);
    fn = ami_openmsg(addr, PORT_TRUNC, FALSE);
    ami_wrmsg(fn, "Hello, server", 13);
    memset(five, 0, sizeof(five));
    len = ami_rdmsg(fn, five, sizeof(five));
    pass = len == 5 && !strncmp(five, "Hello", 5);
    ami_clsmsg(fn);
    finish();
    result("rdmsg short buffer truncates", pass);
    printf("    13 byte message into a 5 byte buffer: %lld bytes, "
           "\"%.5s\"\n", AMI_LONG_CAST(len), five);

}

/* Two connections at once: two servers up together, both clients open,
   the exchanges interleaved -- written in one order, read in the other --
   and each reply is its own server's. One connection at a time is all the
   other tests ever hold, and a mail program holds two. */
static void tsimul(void)

{

    ami_ulong addr;
    FILE* f1;
    FILE* f2;
    char  b1[BUFLEN];
    char  b2[BUFLEN];
    int   pass;

    startsrv(tcpserver, PORT_SIM1, FALSE);
    srv2port = PORT_SIM2;
    cli2done = FALSE;
    srv2done = FALSE;
    ami_newthread(tcpserver2);
    ami_addrnet("localhost", &addr);
    f1 = ami_opennet(addr, PORT_SIM1, FALSE);
    f2 = ami_opennet(addr, PORT_SIM2, FALSE);
    fputs("Hello, server 2\n", f2);
    fflush(f2);
    fputs("Hello, server 1\n", f1);
    fflush(f1);
    b1[0] = b2[0] = 0;
    if (!fgets(b1, BUFLEN, f1)) b1[0] = 0;
    if (!fgets(b2, BUFLEN, f2)) b2[0] = 0;
    pass = !strcmp(b1, "Hello, client\n") && !strcmp(b2, "Hello, client 2\n");
    fclose(f1);
    fclose(f2);
    clientdone();
    ami_lock(lockid);
    cli2done = TRUE;
    ami_sendsig(sigid);
    ami_unlock(lockid);
    finish();
    ami_lock(lockid);
    while (!srv2done) ami_waitsig(lockid, sigid);
    ami_unlock(lockid);
    result("two connections at once", pass);
    printf("    two servers, two clients, replies kept to their own "
           "connections\n");

}

/* The refused connection retry, run on purpose: the server sleeps two
   seconds before it listens and the client connects immediately, so the
   client's first connects are refused and the retry in opennet is what
   makes the exchange succeed. The other TCP tests leave this to a race
   the client usually loses. */
static void tretry(void)

{

    ami_ulong addr;
    FILE* f;
    char  buff[BUFLEN];
    int   pass;

    pass = FALSE;
    startsrv(tcpslowserver, PORT_RETRY, FALSE);
    ami_addrnet("localhost", &addr);
    f = ami_opennet(addr, PORT_RETRY, FALSE);
    fputs("Hello, server\n", f);
    fflush(f);
    if (fgets(buff, BUFLEN, f))
        pass = !strcmp(buff, "Hello, client\n");
    fclose(f);
    clientdone();
    finish();
    result("opennet retries a refused connection", pass);
    printf("    server listened two seconds late; the exchange still "
           "completed\n");

}

/* test 7: certificate of our own secure server */
static void tcert(void)

{

    ami_ulong addr;
    FILE* f;
    char  cert[10000];
    char  buff[BUFLEN];
    ami_long  r;
    ami_certptr list;
    ami_certptr cp;
    int   found;

    startsrv(tcpserver, PORT_CERT, TRUE);
    ami_addrnet("localhost", &addr);
    f = ami_opennet(addr, PORT_CERT, TRUE);

    /* raw certificate is retrievable and non-empty */
    memset(cert, 0, sizeof(cert));
    r = ami_certnet(f, 1, cert, sizeof(cert));
    result("certnet raw certificate", strlen(cert) > 0);
    printf("    TLS server certificate PEM: %lld bytes\n", AMI_LONG_CAST(r));

    /* decoded certificate list: the root is "Certificate" with a data fork,
       and the certificate level signature value is present and non-empty */
    list = NULL;
    ami_certlistnet(f, 1, &list);
    result("certlistnet decodes", list && list->name &&
           !strcmp(list->name, "Certificate") && list->fork);
    found = 0;
    if (list && list->fork)
        for (cp = list->fork; cp; cp = cp->next)
            if (cp->name && !strcmp(cp->name, "Signature Value") &&
                cp->data && cp->data[0]) found = 1;
    result("certlist signature value", found);
    cp = fndnode(list, "Subject");
    printf("    certificate 1 subject: %s\n",
           cp && cp->data ? cp->data : "<none>");
    cp = fndnode(list, "Signature Algorithm");
    printf("    certificate 1 signature algorithm: %s\n",
           cp && cp->data ? cp->data : "<none>");
    /* the leaf is issued by our test CA */
    cp = fndnode(list, "Issuer");
    result("certlist leaf issuer is test CA",
           cp && cp->data && strstr(cp->data, "Petit Ami Test CA") != NULL);
    printf("    certificate 1 issuer: %s\n",
           cp && cp->data ? cp->data : "<none>");
    ami_certlistfree(&list);
    result("certlistfree clears list", list == NULL);

    /* The critical buffer clause, at the boundary that defines it: a
       result that exactly fills the buffer is left without a terminating
       zero, and one byte more terminates. The certificate was fetched
       above into a large buffer, so its length is known; a sentinel
       beyond the passed length proves nothing was written past it. */
    {

        char  exact[10000];
        ami_long  l = r; /* the certificate length, from the fetch above */
        ami_long  r2;

        memset(exact, 0x7f, sizeof(exact));
        r2 = ami_certnet(f, 1, exact, l);
        result("certnet exact fit fills, no terminator",
               r2 == l && exact[l-1] != 0 && exact[l] == 0x7f &&
               !memcmp(exact, cert, l));
        memset(exact, 0x7f, sizeof(exact));
        r2 = ami_certnet(f, 1, exact, l+1);
        result("certnet one byte spare is terminated",
               r2 == l && exact[l] == 0);
        printf("    certificate is %lld bytes: at %lld unterminated, "
               "at %lld terminated\n", AMI_LONG_CAST(l), AMI_LONG_CAST(l), AMI_LONG_CAST(l+1));

    }

    /* the server presents the chain: certificate 2 is the test CA */
    memset(cert, 0, sizeof(cert));
    r = ami_certnet(f, 2, cert, sizeof(cert));
    result("certnet chain CA raw", strlen(cert) > 0);
    list = NULL;
    ami_certlistnet(f, 2, &list);
    cp = fndnode(list, "Subject");
    result("certlistnet chain CA decodes",
           cp && cp->data && strstr(cp->data, "Petit Ami Test CA") != NULL);
    printf("    certificate 2 subject: %s\n",
           cp && cp->data ? cp->data : "<none>");
    /* the CA carries a critical basic constraints CA:TRUE; checks the
       critical flag propagates through the decode */
    cp = fndnode(list, "X509v3 Basic Constraints");
    result("certlist CA basic constraints critical",
           cp && cp->critical && cp->data && strstr(cp->data, "CA:TRUE") != NULL);
    ami_certlistfree(&list);

    /* past the end of the chain: raw form returns zero length, decoded form
       returns an empty list, neither errors */
    memset(cert, 0, sizeof(cert));
    r = ami_certnet(f, 3, cert, sizeof(cert));
    result("certnet past chain end is empty", r == 0 && cert[0] == 0);
    list = NULL;
    ami_certlistnet(f, 3, &list);
    result("certlistnet past chain end is NULL", list == NULL);

    /* complete the exchange so the server exits; the response content is
       not checked here, only consumed */
    fputs("Hello, server\n", f);
    fflush(f);
    if (!fgets(buff, BUFLEN, f)) buff[0] = 0;
    fclose(f);
    clientdone();
    finish();

}

int main(int argc, char **argv)

{

    printf("Network test vs. 0.1\n");
    printf("\n");

    lockid = ami_initlock();
    sigid = ami_initsig();

    section("Name resolution: addrnet (IPv4), addrnetv6 (IPv6); the live "
            "DNS checks need name service, no service stops the test here");
    taddrnet();
    taddrnetv6();
    taddrdns();
    section("Stream connections: opennet/waitnet (IPv4), opennetv6 (IPv6), "
            "opennetname, clear and TLS; certvalidnet on the named TLS run");
    ttcp("TCP exchange in the clear", PORT_TCP, FALSE);
    ttcp("TCP exchange secured (TLS)", PORT_TLS, TRUE);
    ttcpv6("TCP exchange in the clear (IPv6)", PORT_TCP6, FALSE);
    ttcpv6("TCP exchange secured (TLS, IPv6)", PORT_TLS6, TRUE);
    tname("TCP exchange in the clear by name", PORT_NAME, FALSE);
    tname("TCP exchange secured (TLS) by name", PORT_NAMES, TRUE);
    tsimul();
    tretry();
    section("Messages: openmsg/waitmsg/wrmsg/rdmsg/clsmsg (IPv4), openmsgv6 "
            "(IPv6), clear and DTLS; certmsg/certlistmsg on the DTLS run");
    tmsg("message exchange in the clear", PORT_MSG, FALSE);
    tmsg("message exchange secured (DTLS)", PORT_DTLS, TRUE);
    tmsgv6("message exchange in the clear (IPv6)", PORT_MSG6, FALSE);
    tmsgv6("message exchange secured (DTLS, IPv6)", PORT_DTLS6, TRUE);
    trdtrunc();
    section("Message limits: maxmsg/relymsg (IPv4), maxmsgv6/relymsgv6 "
            "(IPv6); a maxmsg sized message is deliverable");
    tmsglim();
    tmsglimv6();
    tmsgmax("maxmsg sized message deliverable", PORT_MAXM, FALSE);
    tmsgmax("maxmsgv6 sized message deliverable", PORT_MAXM6, TRUE);
    section("Certificates: certnet/certlistnet/certlistfree, two deep chain");
    tcert();

    printf("\n");
    printf("Network test complete: %d passes, %d fails\n", passes, fails);

    return (fails != 0);

}
