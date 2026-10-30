/*******************************************************************************
*                                                                              *
*                        REMOTE DISPLAY PROTOCOL HEADER                        *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
*                            2026/08/14 S. A. Franco                           *
*                                                                              *
* The wire protocol for remote display mode, shared by the two modules that    *
* implement it:                                                                *
*                                                                              *
*     graph_client  exposes the complete graphics.h API to the program. Each   *
*                   call composes a message and sends it; event() returns the  *
*                   next inbound event. The program runs where the client is.  *
*                                                                              *
*     graph_server  owns the display. It plugs into the standard graphics      *
*                   package, executes inbound call messages against it, and    *
*                   sends the events back out.                                 *
*                                                                              *
* The display, mouse, keyboard and joystick face the user on the server side,  *
* while the program logic runs on the client side, which is why the            *
* arrangement is sometimes considered "upside down" relative to the usual      *
* client/server naming.                                                        *
*                                                                              *
* CHANNELS                                                                     *
*                                                                              *
* The link is two message channels of network.h, on adjacent ports:            *
*                                                                              *
*     command channel (port p):    client -> server call messages, and         *
*                                  server -> client replies to queries.        *
*     event channel   (port p+1):  server -> client events, continuously.      *
*                                                                              *
* The server runs the command channel on its main thread: wait for a message,  *
* execute it against the display library, reply if the call returns values.    *
* A second thread runs the event channel: call event() forever and send each   *
* event out, as soon as it is ready. Events are pushed, never polled: a pull   *
* model would cost a round trip per event. The client mirrors the push with a  *
* receiver thread of its own, which moves inbound events to the client's       *
* queue as they arrive; event() takes from the queue, and the program          *
* regulates its consumption there. The queue is also where the anticipated     *
* optimization of collapsing redundant events belongs. Calls stay on the       *
* program's thread.                                                            *
*                                                                              *
* Queries are synchronous: the client sends the request and waits for the      *
* reply on the command channel before issuing anything else. There is thus     *
* at most one request outstanding, and replies cannot interleave. The seq      *
* field carries a serial number that the reply echoes, as a consistency        *
* check.                                                                       *
*                                                                              *
* WIRE FORMAT                                                                  *
*                                                                              *
* A message-channel datagram carries one or more messages, back to back,       *
* their header lengths delimiting them; a receiver executes them in order.     *
* No message is split across datagrams. A datagram must fit the channel        *
* maximum given by maxmsg() for the address; senders chunk the write stream    *
* as needed, and no other message can exceed the maximum with its payload      *
* strings, which bounds practical string lengths. Batching is the sender's     *
* option: nothing may wait to fill a batch, and every boundary, a query, an    *
* event wait, the sync, the bye, must send what has gathered at once. All      *
* multibyte values are little endian. The notation used in the message         *
* catalog below:                                                               *
*                                                                              *
*     i64   signed 64 bit integer (the API long)                               *
*     f64   IEEE 754 double (carries the API float values)                     *
*     str   i32 byte length, then the bytes, no terminator                     *
*     blk   i32 byte length, then the bytes                                    *
*     slst  string list: i32 count, then count str                             *
*     menu  menu tree: i32 count, then count entries, each entry               *
*           { i64 flags (bit 0 on/off capable, bit 1 oneof, bit 2 bar),        *
*             i64 id, str face, then a nested menu for the branch }            *
*     evt   event record: 11 i64: window handle, event code, handled, then     *
*           eight parameter slots p0..p7 holding the event's union fields in   *
*           declaration order (a character rides in p0)                        *
*                                                                              *
* Each message begins with the fixed header:                                   *
*                                                                              *
*     i32 len   total message length in bytes, header included                 *
*     i16 mid   message id, from the catalog below                             *
*     i16 seq   request serial; echoed by the reply; 0 where not meaningful    *
*     i64 wid   window handle the call acts on; 0 where not meaningful         *
*                                                                              *
* followed by the payload fields in the order the API call declares them,      *
* window file parameter excluded (it is the header wid).                       *
*                                                                              *
* WINDOW HANDLES                                                               *
*                                                                              *
* The client assigns window handles: handle 1 is the main window (the          *
* standard input/output pair), and openwin() takes the parent handle, the      *
* new handle, and the logical window id. The server maps handles to its own    *
* window files. A window closes with GR_MCLOSEWIN.                             *
*                                                                              *
* SCOPE                                                                        *
*                                                                              *
* The mouse, the keyboard and the joysticks are the server's; their events     *
* flow to the client, and the input device queries are synchronous queries     *
* like any other. eventover(), eventsover() and sendevent() act on the         *
* client's own event stream and put nothing on the wire. Print files           *
* (openprint()) are reserved for a later protocol version.                     *
*                                                                              *
* FILE TRANSFER                                                                *
*                                                                              *
* Files named in calls, the picture files of loadpict(), live where the        *
* program lives, on the client; the server holds a cache, its current          *
* directory. When a call names a file the server looks it up: first the name   *
* as given, then the base name in the cache. If it holds the file the call     *
* proceeds. If not, it requests the file from the client, on the model of      *
* ftp's passive form: the request goes by message inside the pending call's    *
* reply window, naming the file and the server's file port, the command       *
* port + 2. The client opens that port as a stream connection (opennet())     *
* and streams the file in, raw bytes, close delimited; an empty stream is a   *
* file the client does not hold either. The received file is stored in the    *
* cache under its base name, never under a path the client supplied, and the  *
* call then proceeds against the cache and sends its reply.                    *
*                                                                              *
* The passive form is used because the message channels do not disclose the    *
* client's address to the server, while the client always knows the            *
* server's; it is also the form that crosses address translation toward the   *
* server. One transfer runs at a time, which the one outstanding request       *
* rule already guarantees. The mechanism is not tied to pictures: any call     *
* that names a file uses the same exchange.                                    *
*                                                                              *
* RELIABILITY                                                                  *
*                                                                              *
* The protocol requires a reliable channel, in the sense of relymsg():         *
* no loss, no reordering. The local machine connection of the standard test    *
* setup is reliable by definition. Operation over an unreliable network      *
* needs a retransmission layer under this protocol, which is future work.      *
*                                                                              *
*******************************************************************************/

#ifndef __GRAPH_REMOTE_H__
#define __GRAPH_REMOTE_H__
#include <localdefs.h>

#define GR_VERSION 1    /* protocol version, carried in the hello exchange */
#define GR_DEFPORT 4901 /* default command port; the event channel is +1,
                           the file transfer port +2 */

/* the fixed message header. The types are chosen for the wire widths on the
   supported platforms: int is 32 bits, short 16, ami_long 64, little endian. */
typedef struct gr_msghdr {

    int   len; /* total message length, header included */
    short mid; /* message id */
    short seq; /* request serial, echoed by the reply */
    ami_long  wid; /* window handle, 0 if not meaningful */

} gr_msghdr;

/* the event record as it rides the wire, after the header */
typedef struct gr_msgevt {

    ami_long winid;  /* window handle the event belongs to */
    ami_long etype;  /* event code */
    ami_long handled;/* handled flag */
    ami_long p[8];   /* the union fields, in declaration order */

} gr_msgevt;

/*
 * The message catalog. Each entry gives the payload after the header, and
 * the reply payload where the call returns values; calls without a reply
 * are not acknowledged. Replies use GR_MREPLY with the request's seq.
 */
typedef enum {

    /* --------------------------------------------------------- session */

    /** first on the command channel: i64 version -> i64 version. The
       server refuses a version it does not speak by replying its own and
       closing. */
    GR_MHELLO      = 1,
    /** first on the event channel, from the client, so the server learns
       where events go: i64 version. No reply. */
    GR_MEVOPEN     = 2,
    /** client is done; the server drops the connection. No payload. */
    GR_MBYE        = 3,
    /** server -> client on the event channel: str message. The client
       raises the error to the program. */
    GR_MERROR      = 4,
    /** server -> client on the command channel, inside the reply window
       of a pending call that names a file the server does not hold:
       str name, i64 port, the server's file port. The client opens the
       port as a stream connection and streams the file in, raw bytes,
       close delimited; the pending call then completes and replies. */
    GR_MFILEREQ    = 5,
    /** reserved: the active form of the transfer, where the server
       would connect to a port the client names. The passive form above
       is the form of this protocol version. */
    GR_MFILEPORT   = 6,
    /** client -> server: -> i64 0. The flow fence: the client bounds
       its outstanding bytes by syncing once per window, so a command
       burst cannot outrun the server's socket into datagram loss. Any
       query is also a fence, being a round trip. */
    GR_MSYNC       = 7,
    /** the reply to any query: the request's seq, and the payload the
       request's catalog entry gives. */
    GR_MREPLY      = 9,

    /* ---------------------------------------------------- byte stream */

    /** the write stream of the window: blk of characters, as they would
       go to the window file. Chunked by the sender to the channel
       maximum. No reply. */
    GR_MWRITE      = 10,
    /** the window file closes: the server closes its side. No payload. */
    GR_MCLOSEWIN   = 11,

    /* ------------------------------------------------- terminal level */

    GR_MCURSOR     = 20,  /* i64 x, i64 y */
    GR_MMAXX       = 21,  /* -> i64 */
    GR_MMAXY       = 22,  /* -> i64 */
    GR_MHOME       = 23,  /* */
    GR_MDEL        = 24,  /* */
    GR_MUP         = 25,  /* */
    GR_MDOWN       = 26,  /* */
    GR_MLEFT       = 27,  /* */
    GR_MRIGHT      = 28,  /* */
    GR_MBLINK      = 29,  /* i64 e */
    GR_MREVERSE    = 30,  /* i64 e */
    GR_MUNDERLINE  = 31,  /* i64 e */
    GR_MSUPERSCRIPT= 32,  /* i64 e */
    GR_MSUBSCRIPT  = 33,  /* i64 e */
    GR_MITALIC     = 34,  /* i64 e */
    GR_MBOLD       = 35,  /* i64 e */
    GR_MSTRIKEOUT  = 36,  /* i64 e */
    GR_MSTANDOUT   = 37,  /* i64 e */
    GR_MFCOLOR     = 38,  /* i64 color */
    GR_MBCOLOR     = 39,  /* i64 color */
    GR_MAUTO       = 40,  /* i64 e */
    GR_MCURVIS     = 41,  /* i64 e */
    GR_MSCROLL     = 42,  /* i64 x, i64 y */
    GR_MCURX       = 43,  /* -> i64 */
    GR_MCURY       = 44,  /* -> i64 */
    GR_MCURBND     = 45,  /* -> i64 */
    GR_MSELECT     = 46,  /* i64 u, i64 d */
    GR_MTIMER      = 47,  /* i64 i, i64 t, i64 r */
    GR_MKILLTIMER  = 48,  /* i64 i */
    GR_MMOUSE      = 49,  /* -> i64 */
    GR_MMOUSEBUTTON= 50,  /* i64 m -> i64 */
    GR_MJOYSTICK   = 51,  /* -> i64 */
    GR_MJOYBUTTON  = 52,  /* i64 j -> i64 */
    GR_MJOYAXIS    = 53,  /* i64 j -> i64 */
    GR_MSETTAB     = 54,  /* i64 t */
    GR_MRESTAB     = 55,  /* i64 t */
    GR_MCLRTAB     = 56,  /* */
    GR_MFUNKEY     = 57,  /* -> i64 */
    GR_MFRAMETIMER = 58,  /* i64 e */
    GR_MAUTOHOLD   = 59,  /* i64 e; wid 0, the hold is global */
    GR_MWRTSTR     = 60,  /* str */
    GR_MWRTSTRN    = 61,  /* str */
    GR_MSIZBUF     = 62,  /* i64 x, i64 y */
    GR_MTITLE      = 63,  /* str */
    GR_MFCOLORC    = 64,  /* i64 r, i64 g, i64 b */
    GR_MBCOLORC    = 65,  /* i64 r, i64 g, i64 b */

    /* ------------------------------------------------ graphical level */

    GR_MMAXXG      = 100, /* -> i64 */
    GR_MMAXYG      = 101, /* -> i64 */
    GR_MCURXG      = 102, /* -> i64 */
    GR_MCURYG      = 103, /* -> i64 */
    GR_MLINE       = 104, /* i64 x1, y1, x2, y2 */
    GR_MLINEWIDTH  = 105, /* i64 w */
    GR_MLINESTYLE  = 106, /* i64 style */
    GR_MRECT       = 107, /* i64 x1, y1, x2, y2 */
    GR_MFRECT      = 108, /* i64 x1, y1, x2, y2 */
    GR_MRRECT      = 109, /* i64 x1, y1, x2, y2, xs, ys */
    GR_MFRRECT     = 110, /* i64 x1, y1, x2, y2, xs, ys */
    GR_MELLIPSE    = 111, /* i64 x1, y1, x2, y2 */
    GR_MFELLIPSE   = 112, /* i64 x1, y1, x2, y2 */
    GR_MARC        = 113, /* i64 x1, y1, x2, y2, sa, ea */
    GR_MFARC       = 114, /* i64 x1, y1, x2, y2, sa, ea */
    GR_MFCHORD     = 115, /* i64 x1, y1, x2, y2, sa, ea */
    GR_MFTRIANGLE  = 116, /* i64 x1, y1, x2, y2, x3, y3 */
    GR_MCURSORG    = 117, /* i64 x, i64 y */
    GR_MBASELINE   = 118, /* -> i64 */
    GR_MSETPIXEL   = 119, /* i64 x, i64 y */
    GR_MFOVER      = 120, /* */
    GR_MBOVER      = 121, /* */
    GR_MFINVIS     = 122, /* */
    GR_MBINVIS     = 123, /* */
    GR_MFXOR       = 124, /* */
    GR_MBXOR       = 125, /* */
    GR_MFAND       = 126, /* */
    GR_MBAND       = 127, /* */
    GR_MFOR        = 128, /* */
    GR_MBOR        = 129, /* */
    GR_MCHRSIZX    = 130, /* -> i64 */
    GR_MCHRSIZY    = 131, /* -> i64 */
    GR_MFONTS      = 132, /* -> i64 */
    GR_MFONT       = 133, /* i64 fc */
    GR_MFONTNAM    = 134, /* i64 fc -> str */
    GR_MFONTSIZ    = 135, /* i64 s */
    GR_MSETPOINTS  = 136, /* f64 ps */
    GR_MPOINTS     = 137, /* -> f64 */
    GR_MCHRSPCY    = 138, /* i64 s */
    GR_MCHRSPCX    = 139, /* i64 s */
    GR_MDPMX       = 140, /* -> i64 */
    GR_MDPMY       = 141, /* -> i64 */
    GR_MSTRSIZ     = 142, /* str -> i64 */
    GR_MCHRPOS     = 143, /* str, i64 p -> i64 */
    GR_MWRITEJUST  = 144, /* str, i64 n */
    GR_MJUSTPOS    = 145, /* str, i64 p, i64 n -> i64 */
    GR_MCONDENSED  = 146, /* i64 e */
    GR_MEXTENDED   = 147, /* i64 e */
    GR_MXLIGHT     = 148, /* i64 e */
    GR_MLIGHT      = 149, /* i64 e */
    GR_MXBOLD      = 150, /* i64 e */
    GR_MHOLLOW     = 151, /* i64 e */
    GR_MRAISED     = 152, /* i64 e */
    GR_MSETTABG    = 153, /* i64 t */
    GR_MRESTABG    = 154, /* i64 t */
    GR_MFCOLORG    = 155, /* i64 r, i64 g, i64 b */
    GR_MBCOLORG    = 156, /* i64 r, i64 g, i64 b */
    GR_MLOADPICT   = 157, /* i64 p, str name -> i64 0. The server fills
                             its cache by the file transfer exchange if
                             it does not hold the file; the reply comes
                             after the picture is loaded. */
    GR_MPICTSIZX   = 158, /* i64 p -> i64 */
    GR_MPICTSIZY   = 159, /* i64 p -> i64 */
    GR_MPICTURE    = 160, /* i64 p, x1, y1, x2, y2 */
    GR_MDELPICT    = 161, /* i64 p */
    GR_MSCROLLG    = 162, /* i64 x, i64 y */
    GR_MPATH       = 163, /* i64 a */
    GR_MVIEWOFFG   = 164, /* i64 x, i64 y */
    GR_MVIEWSCALE  = 165, /* f64 x, f64 y */
    GR_MSCALEX     = 166, /* i64 x -> i64 */
    GR_MSCALEY     = 167, /* i64 y -> i64 */
    GR_MBLOCKCOPYG = 168, /* i64 s, d, sx1, sy1, sx2, sy2, dx1, dy1, dx2,
                             dy2 */

    /* ----------------------------------------------- window management */

    /** open a window: i64 parent handle (0 for none), i64 new handle,
       i64 logical window id. The header wid is 0. */
    GR_MOPENWIN    = 200,
    GR_MBUFFER     = 201, /* i64 e */
    GR_MSIZBUFG    = 202, /* i64 x, i64 y */
    GR_MGETSIZ     = 203, /* -> i64 x, i64 y */
    GR_MGETSIZG    = 204, /* -> i64 x, i64 y */
    GR_MSETSIZ     = 205, /* i64 x, i64 y */
    GR_MSETSIZG    = 206, /* i64 x, i64 y */
    GR_MSETPOS     = 207, /* i64 x, i64 y */
    GR_MSETPOSG    = 208, /* i64 x, i64 y */
    GR_MDRAGWIN    = 209, /* */
    GR_MSCNSIZ     = 210, /* -> i64 x, i64 y */
    GR_MSCNSIZG    = 211, /* -> i64 x, i64 y */
    GR_MSCNCEN     = 212, /* -> i64 x, i64 y */
    GR_MSCNCENG    = 213, /* -> i64 x, i64 y */
    GR_MWINCLIENT  = 214, /* i64 cx, cy, ms -> i64 wx, wy */
    GR_MWINCLIENTG = 215, /* i64 cx, cy, ms -> i64 wx, wy */
    GR_MFRONT      = 216, /* */
    GR_MBACK       = 217, /* */
    GR_MFRAME      = 218, /* i64 e */
    GR_MSIZABLE    = 219, /* i64 e */
    GR_MSYSBAR     = 220, /* i64 e */
    GR_MMENU       = 221, /* menu; an empty tree removes the menu */
    GR_MMENUENA    = 222, /* i64 id, i64 e */
    GR_MMENUSEL    = 223, /* i64 id, i64 e */
    /** i64 standard menu set, menu the user additions -> menu, the
       combined menu as the server built it, which the client materializes
       for the program */
    GR_MSTDMENU    = 224,
    GR_MGETWINID   = 225, /* -> i64; wid 0, ids are global */
    GR_MFOCUS      = 226, /* */

    /* ----------------------------------------------------- widgets */

    GR_MGETWIGID   = 300, /* -> i64 */
    GR_MKILLWIDGET = 301, /* i64 id */
    GR_MSELECTWIDGET = 302, /* i64 id, i64 e */
    GR_MENABLEWIDGET = 303, /* i64 id, i64 e */
    GR_MGETWIDGETTEXT = 304, /* i64 id -> str */
    GR_MPUTWIDGETTEXT = 305, /* i64 id, str */
    GR_MSIZWIDGET  = 306, /* i64 id, i64 x, i64 y */
    GR_MSIZWIDGETG = 307, /* i64 id, i64 x, i64 y */
    GR_MPOSWIDGET  = 308, /* i64 id, i64 x, i64 y */
    GR_MPOSWIDGETG = 309, /* i64 id, i64 x, i64 y */
    GR_MBACKWIDGET = 310, /* i64 id */
    GR_MFRONTWIDGET= 311, /* i64 id */
    GR_MFOCUSWIDGET= 312, /* i64 id */
    GR_MBUTTONSIZ  = 313, /* str -> i64 w, i64 h */
    GR_MBUTTONSIZG = 314, /* str -> i64 w, i64 h */
    GR_MBUTTON     = 315, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MBUTTONG    = 316, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MCHECKBOXSIZ= 317, /* str -> i64 w, i64 h */
    GR_MCHECKBOXSIZG = 318, /* str -> i64 w, i64 h */
    GR_MCHECKBOX   = 319, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MCHECKBOXG  = 320, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MRADIOBUTTONSIZ = 321, /* str -> i64 w, i64 h */
    GR_MRADIOBUTTONSIZG = 322, /* str -> i64 w, i64 h */
    GR_MRADIOBUTTON = 323, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MRADIOBUTTONG = 324, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MGROUPSIZG  = 325, /* str, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MGROUPSIZ   = 326, /* str, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MGROUP      = 327, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MGROUPG     = 328, /* i64 x1, y1, x2, y2, str, i64 id */
    GR_MBACKGROUND = 329, /* i64 x1, y1, x2, y2, i64 id */
    GR_MBACKGROUNDG= 330, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLVERTSIZG = 331, /* -> i64 w, i64 h */
    GR_MSCROLLVERTSIZ = 332, /* -> i64 w, i64 h */
    GR_MSCROLLVERT = 333, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLVERTG= 334, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLHORIZSIZG = 335, /* -> i64 w, i64 h */
    GR_MSCROLLHORIZSIZ = 336, /* -> i64 w, i64 h */
    GR_MSCROLLHORIZ= 337, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLHORIZG = 338, /* i64 x1, y1, x2, y2, i64 id */
    GR_MSCROLLPOS  = 339, /* i64 id, i64 r */
    GR_MSCROLLSIZ  = 340, /* i64 id, i64 r */
    GR_MNUMSELBOXSIZG = 341, /* i64 l, i64 u -> i64 w, i64 h */
    GR_MNUMSELBOXSIZ = 342, /* i64 l, i64 u -> i64 w, i64 h */
    GR_MNUMSELBOX  = 343, /* i64 x1, y1, x2, y2, l, u, id */
    GR_MNUMSELBOXG = 344, /* i64 x1, y1, x2, y2, l, u, id */
    GR_MEDITBOXSIZG= 345, /* str -> i64 w, i64 h */
    GR_MEDITBOXSIZ = 346, /* str -> i64 w, i64 h */
    GR_MEDITBOX    = 347, /* i64 x1, y1, x2, y2, i64 id */
    GR_MEDITBOXG   = 348, /* i64 x1, y1, x2, y2, i64 id */
    GR_MPROGBARSIZG= 349, /* -> i64 w, i64 h */
    GR_MPROGBARSIZ = 350, /* -> i64 w, i64 h */
    GR_MPROGBAR    = 351, /* i64 x1, y1, x2, y2, i64 id */
    GR_MPROGBARG   = 352, /* i64 x1, y1, x2, y2, i64 id */
    GR_MPROGBARPOS = 353, /* i64 id, i64 pos */
    GR_MLISTBOXSIZG= 354, /* slst -> i64 w, i64 h */
    GR_MLISTBOXSIZ = 355, /* slst -> i64 w, i64 h */
    GR_MLISTBOX    = 356, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MLISTBOXG   = 357, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPBOXSIZG= 358, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPBOXSIZ = 359, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPBOX    = 360, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPBOXG   = 361, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPEDITBOXSIZG = 362, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPEDITBOXSIZ = 363, /* slst -> i64 cw, ch, ow, oh */
    GR_MDROPEDITBOX = 364, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MDROPEDITBOXG = 365, /* i64 x1, y1, x2, y2, slst, i64 id */
    GR_MSLIDEHORIZSIZG = 366, /* -> i64 w, i64 h */
    GR_MSLIDEHORIZSIZ = 367, /* -> i64 w, i64 h */
    GR_MSLIDEHORIZ = 368, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MSLIDEHORIZG= 369, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MSLIDEVERTSIZG = 370, /* -> i64 w, i64 h */
    GR_MSLIDEVERTSIZ = 371, /* -> i64 w, i64 h */
    GR_MSLIDEVERT  = 372, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MSLIDEVERTG = 373, /* i64 x1, y1, x2, y2, i64 mark, i64 id */
    GR_MTABBARSIZG = 374, /* slst, i64 tor, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MTABBARSIZ  = 375, /* slst, i64 tor, i64 cw, i64 ch -> i64 w, h, ox, oy */
    GR_MTABBARCLIENTG = 376, /* i64 tor, i64 w, i64 h -> i64 cw, ch, ox, oy */
    GR_MTABBARCLIENT = 377, /* i64 tor, i64 w, i64 h -> i64 cw, ch, ox, oy */
    GR_MTABBAR     = 378, /* i64 x1, y1, x2, y2, slst, i64 tor, i64 id */
    GR_MTABBARG    = 379, /* i64 x1, y1, x2, y2, slst, i64 tor, i64 id */
    GR_MTABSEL     = 380, /* i64 id, i64 tn */

    /* ----------------------------------------------------- dialogs */

    GR_MALERT      = 420, /* str title, str message -> nothing; the reply
                              comes when the dialog is dismissed, which is
                              what makes the call modal at the far end */
    GR_MQUERYCOLOR = 421, /* i64 r, g, b -> i64 r, g, b; wid 0 */
    GR_MQUERYOPEN  = 422, /* str -> str; wid 0 */
    GR_MQUERYSAVE  = 423, /* str -> str; wid 0 */
    GR_MQUERYFIND  = 424, /* str, i64 opt -> str, i64 opt; wid 0 */
    GR_MQUERYFINDREP = 425, /* str, str, i64 opt -> str, str, i64 opt;
                               wid 0 */
    GR_MQUERYFONT  = 426, /* i64 fc, s, fr, fg, fb, br, bg, bb, effect ->
                             the same nine, as chosen */

    /* ----------------------------------------------------- test rig */

    /** The picture tests' capture hook and the event file's seat, which
       ride the display side (tests/auto_event.c, the screen_capture
       modules): the server takes the pictures of the session's window
       and puts the seat's lines to its display's rig. A capture is
       answered when the picture is written, so the test stands until it
       is, as it does with a display of its own. */
    GR_MCAPTURENAME = 480, /* str: the file the pictures go to */
    GR_MCAPTURELABEL = 481, /* str: the next picture's title */
    GR_MCAPTURE    = 482, /* -> i64 0, when the picture is written */
    GR_MSEAT       = 483, /* str: a rig line; its pointer coordinates are
                             in the session window's client area, and the
                             server places them on its display */
    GR_MAUTOBUF    = 484, /* i64 e */

    /* ----------------------------------------------------- events */

    /** an event, server -> client on the event channel: evt */
    GR_MEVENT      = 500,

    /* ------------------------------------------------------------------
       The module partition. The i16 message id space divides into four
       modules of 8192 codes each: graphics holds 0..8191 (the ids
       above), sound holds 8192..16383, and two partitions remain for
       future modules. The negative half of the space is reserved. A
       module's calls may grow within its partition without disturbing
       its neighbors.
       ------------------------------------------------------------------ */

    GR_SNDBASE     = 8192, /* the sound partition */

    /* sequencer time bases */
    GR_MSTARTTIMEOUT   = GR_SNDBASE+0,  /* -- */
    GR_MSTOPTIMEOUT    = GR_SNDBASE+1,  /* -- */
    GR_MCURTIMEOUT     = GR_SNDBASE+2,  /* -> i64 time */
    GR_MSTARTTIMEIN    = GR_SNDBASE+3,  /* -- */
    GR_MSTOPTIMEIN     = GR_SNDBASE+4,  /* -- */
    GR_MCURTIMEIN      = GR_SNDBASE+5,  /* -> i64 time */

    /* device counts and opens */
    GR_MSYNTHOUT       = GR_SNDBASE+6,  /* -> i64 count */
    GR_MSYNTHIN        = GR_SNDBASE+7,  /* -> i64 count */
    GR_MOPENSYNTHOUT   = GR_SNDBASE+8,  /* i64 p */
    GR_MCLOSESYNTHOUT  = GR_SNDBASE+9,  /* i64 p */
    GR_MOPENSYNTHIN    = GR_SNDBASE+10, /* i64 p */
    GR_MCLOSESYNTHIN   = GR_SNDBASE+11, /* i64 p */

    /* the channel voice messages: i64 p, t, c, then the values */
    GR_MNOTEON         = GR_SNDBASE+12, /* i64 p, t, c, n, v */
    GR_MNOTEOFF        = GR_SNDBASE+13, /* i64 p, t, c, n, v */
    GR_MINSTCHANGE     = GR_SNDBASE+14, /* i64 p, t, c, i */
    GR_MATTACK         = GR_SNDBASE+15, /* i64 p, t, c, at */
    GR_MRELEASE        = GR_SNDBASE+16, /* i64 p, t, c, rt */
    GR_MLEGATO         = GR_SNDBASE+17, /* i64 p, t, c, b */
    GR_MPORTAMENTO     = GR_SNDBASE+18, /* i64 p, t, c, b */
    GR_MVIBRATO        = GR_SNDBASE+19, /* i64 p, t, c, v */
    GR_MVOLSYNTHCHAN   = GR_SNDBASE+20, /* i64 p, t, c, v */
    GR_MPORTTIME       = GR_SNDBASE+21, /* i64 p, t, c, v */
    GR_MBALANCE        = GR_SNDBASE+22, /* i64 p, t, c, b */
    GR_MPAN            = GR_SNDBASE+23, /* i64 p, t, c, b */
    GR_MTIMBRE         = GR_SNDBASE+24, /* i64 p, t, c, tb */
    GR_MBRIGHTNESS     = GR_SNDBASE+25, /* i64 p, t, c, b */
    GR_MREVERB         = GR_SNDBASE+26, /* i64 p, t, c, r */
    GR_MTREMULO        = GR_SNDBASE+27, /* i64 p, t, c, tr */
    GR_MCHORUS         = GR_SNDBASE+28, /* i64 p, t, c, cr */
    GR_MCELESTE        = GR_SNDBASE+29, /* i64 p, t, c, ce */
    GR_MPHASER         = GR_SNDBASE+30, /* i64 p, t, c, ph */
    GR_MAFTERTOUCH     = GR_SNDBASE+31, /* i64 p, t, c, n, at */
    GR_MPRESSURE       = GR_SNDBASE+32, /* i64 p, t, c, pr */
    GR_MPITCH          = GR_SNDBASE+33, /* i64 p, t, c, pt */
    GR_MPITCHRANGE     = GR_SNDBASE+34, /* i64 p, t, c, v */
    GR_MMONO           = GR_SNDBASE+35, /* i64 p, t, c, ch */
    GR_MPOLY           = GR_SNDBASE+36, /* i64 p, t, c */

    /* stored sequences. loadsynth is a query for the same reason
       loadpict is: the file transfer request must happen inside the
       reply window, with the client listening. */
    GR_MLOADSYNTH      = GR_SNDBASE+37, /* i64 s, str fn -> i64 0 */
    GR_MPLAYSYNTH      = GR_SNDBASE+38, /* i64 p, t, s */
    GR_MDELSYNTH       = GR_SNDBASE+39, /* i64 s */
    GR_MWAITSYNTH      = GR_SNDBASE+40, /* i64 p -> i64 0, on completion */
    GR_MWRSYNTH        = GR_SNDBASE+41, /* i64 p + seq: i64 port, time,
                                           st, p1, p2, p3 */
    GR_MRDSYNTH        = GR_SNDBASE+42, /* i64 p -> seq; blocks the caller
                                           until input arrives, as the
                                           native call does */

    /* wave output */
    GR_MWAVEOUT        = GR_SNDBASE+43, /* -> i64 count */
    GR_MWAVEIN         = GR_SNDBASE+44, /* -> i64 count */
    GR_MOPENWAVEOUT    = GR_SNDBASE+45, /* i64 p */
    GR_MCLOSEWAVEOUT   = GR_SNDBASE+46, /* i64 p */
    GR_MLOADWAVE       = GR_SNDBASE+47, /* i64 w, str fn -> i64 0 */
    GR_MPLAYWAVE       = GR_SNDBASE+48, /* i64 p, t, w */
    GR_MDELWAVE        = GR_SNDBASE+49, /* i64 w */
    GR_MVOLWAVE        = GR_SNDBASE+50, /* i64 p, t, v */
    GR_MWAITWAVE       = GR_SNDBASE+51, /* i64 p -> i64 0, on completion */
    GR_MCHANWAVEOUT    = GR_SNDBASE+52, /* i64 p, c */
    GR_MRATEWAVEOUT    = GR_SNDBASE+53, /* i64 p, r */
    GR_MLENWAVEOUT     = GR_SNDBASE+54, /* i64 p, l */
    GR_MSGNWAVEOUT     = GR_SNDBASE+55, /* i64 p, s */
    GR_MFLTWAVEOUT     = GR_SNDBASE+56, /* i64 p, f */
    GR_MENDWAVEOUT     = GR_SNDBASE+57, /* i64 p, e */
    GR_MWRWAVE         = GR_SNDBASE+58, /* i64 p, i64 frames, blk
                                           samples -> i64 0. The sender
                                           chunks to the channel bound on
                                           frame boundaries; each chunk
                                           is acknowledged, which paces
                                           the stream to the consumer and
                                           keeps it whole. */

    /* wave input; the devices live at the server, and a read blocks the
       caller until data arrives, as the native call does */
    GR_MOPENWAVEIN     = GR_SNDBASE+59, /* i64 p */
    GR_MCLOSEWAVEIN    = GR_SNDBASE+60, /* i64 p */
    GR_MCHANWAVEIN     = GR_SNDBASE+61, /* i64 p -> i64 */
    GR_MRATEWAVEIN     = GR_SNDBASE+62, /* i64 p -> i64 */
    GR_MLENWAVEIN      = GR_SNDBASE+63, /* i64 p -> i64 */
    GR_MSGNWAVEIN      = GR_SNDBASE+64, /* i64 p -> i64 */
    GR_MENDWAVEIN      = GR_SNDBASE+65, /* i64 p -> i64 */
    GR_MFLTWAVEIN      = GR_SNDBASE+66, /* i64 p -> i64 */
    GR_MRDWAVE         = GR_SNDBASE+67, /* i64 p, frames -> i64 got, blk
                                           samples; the server clamps the
                                           answer to the channel bound and
                                           the caller loops. Lengths are in
                                           samples, one frame of every
                                           channel, as the API counts. */

    /* device names and parameters */
    GR_MSYNTHOUTNAME   = GR_SNDBASE+68, /* i64 p -> str */
    GR_MSYNTHINNAME    = GR_SNDBASE+69, /* i64 p -> str */
    GR_MWAVEOUTNAME    = GR_SNDBASE+70, /* i64 p -> str */
    GR_MWAVEINNAME     = GR_SNDBASE+71, /* i64 p -> str */
    GR_MSETPARAMSYNTHIN  = GR_SNDBASE+72, /* i64 p, str, str -> i64 */
    GR_MSETPARAMSYNTHOUT = GR_SNDBASE+73, /* i64 p, str, str -> i64 */
    GR_MSETPARAMWAVEIN   = GR_SNDBASE+74, /* i64 p, str, str -> i64 */
    GR_MSETPARAMWAVEOUT  = GR_SNDBASE+75, /* i64 p, str, str -> i64 */
    GR_MGETPARAMSYNTHIN  = GR_SNDBASE+76, /* i64 p, str -> str */
    GR_MGETPARAMSYNTHOUT = GR_SNDBASE+77, /* i64 p, str -> str */
    GR_MGETPARAMWAVEIN   = GR_SNDBASE+78, /* i64 p, str -> str */
    GR_MGETPARAMWAVEOUT  = GR_SNDBASE+79  /* i64 p, str -> str */

} gr_msgcod;


/*
 * Diagnostic names, generated from the enums above and from the event
 * codes of graphics.h; regenerate when either changes. The trace modes
 * of the client and server print by these.
 */

static const char* gr_msgname(int mid)
{
    switch (mid) {
        case GR_MHELLO: return "HELLO";
        case GR_MEVOPEN: return "EVOPEN";
        case GR_MBYE: return "BYE";
        case GR_MERROR: return "ERROR";
        case GR_MFILEREQ: return "FILEREQ";
        case GR_MFILEPORT: return "FILEPORT";
        case GR_MREPLY: return "REPLY";
        case GR_MWRITE: return "WRITE";
        case GR_MCLOSEWIN: return "CLOSEWIN";
        case GR_MCURSOR: return "CURSOR";
        case GR_MMAXX: return "MAXX";
        case GR_MMAXY: return "MAXY";
        case GR_MHOME: return "HOME";
        case GR_MDEL: return "DEL";
        case GR_MUP: return "UP";
        case GR_MDOWN: return "DOWN";
        case GR_MLEFT: return "LEFT";
        case GR_MRIGHT: return "RIGHT";
        case GR_MBLINK: return "BLINK";
        case GR_MREVERSE: return "REVERSE";
        case GR_MUNDERLINE: return "UNDERLINE";
        case GR_MSUPERSCRIPT: return "SUPERSCRIPT";
        case GR_MSUBSCRIPT: return "SUBSCRIPT";
        case GR_MITALIC: return "ITALIC";
        case GR_MBOLD: return "BOLD";
        case GR_MSTRIKEOUT: return "STRIKEOUT";
        case GR_MSTANDOUT: return "STANDOUT";
        case GR_MFCOLOR: return "FCOLOR";
        case GR_MBCOLOR: return "BCOLOR";
        case GR_MAUTO: return "AUTO";
        case GR_MCURVIS: return "CURVIS";
        case GR_MSCROLL: return "SCROLL";
        case GR_MCURX: return "CURX";
        case GR_MCURY: return "CURY";
        case GR_MCURBND: return "CURBND";
        case GR_MSELECT: return "SELECT";
        case GR_MTIMER: return "TIMER";
        case GR_MKILLTIMER: return "KILLTIMER";
        case GR_MMOUSE: return "MOUSE";
        case GR_MMOUSEBUTTON: return "MOUSEBUTTON";
        case GR_MJOYSTICK: return "JOYSTICK";
        case GR_MJOYBUTTON: return "JOYBUTTON";
        case GR_MJOYAXIS: return "JOYAXIS";
        case GR_MSETTAB: return "SETTAB";
        case GR_MRESTAB: return "RESTAB";
        case GR_MCLRTAB: return "CLRTAB";
        case GR_MFUNKEY: return "FUNKEY";
        case GR_MFRAMETIMER: return "FRAMETIMER";
        case GR_MAUTOHOLD: return "AUTOHOLD";
        case GR_MWRTSTR: return "WRTSTR";
        case GR_MWRTSTRN: return "WRTSTRN";
        case GR_MSIZBUF: return "SIZBUF";
        case GR_MTITLE: return "TITLE";
        case GR_MFCOLORC: return "FCOLORC";
        case GR_MBCOLORC: return "BCOLORC";
        case GR_MMAXXG: return "MAXXG";
        case GR_MMAXYG: return "MAXYG";
        case GR_MCURXG: return "CURXG";
        case GR_MCURYG: return "CURYG";
        case GR_MLINE: return "LINE";
        case GR_MLINEWIDTH: return "LINEWIDTH";
        case GR_MLINESTYLE: return "LINESTYLE";
        case GR_MRECT: return "RECT";
        case GR_MFRECT: return "FRECT";
        case GR_MRRECT: return "RRECT";
        case GR_MFRRECT: return "FRRECT";
        case GR_MELLIPSE: return "ELLIPSE";
        case GR_MFELLIPSE: return "FELLIPSE";
        case GR_MARC: return "ARC";
        case GR_MFARC: return "FARC";
        case GR_MFCHORD: return "FCHORD";
        case GR_MFTRIANGLE: return "FTRIANGLE";
        case GR_MCURSORG: return "CURSORG";
        case GR_MBASELINE: return "BASELINE";
        case GR_MSETPIXEL: return "SETPIXEL";
        case GR_MFOVER: return "FOVER";
        case GR_MBOVER: return "BOVER";
        case GR_MFINVIS: return "FINVIS";
        case GR_MBINVIS: return "BINVIS";
        case GR_MFXOR: return "FXOR";
        case GR_MBXOR: return "BXOR";
        case GR_MFAND: return "FAND";
        case GR_MBAND: return "BAND";
        case GR_MFOR: return "FOR";
        case GR_MBOR: return "BOR";
        case GR_MCHRSIZX: return "CHRSIZX";
        case GR_MCHRSIZY: return "CHRSIZY";
        case GR_MFONTS: return "FONTS";
        case GR_MFONT: return "FONT";
        case GR_MFONTNAM: return "FONTNAM";
        case GR_MFONTSIZ: return "FONTSIZ";
        case GR_MSETPOINTS: return "SETPOINTS";
        case GR_MPOINTS: return "POINTS";
        case GR_MCHRSPCY: return "CHRSPCY";
        case GR_MCHRSPCX: return "CHRSPCX";
        case GR_MDPMX: return "DPMX";
        case GR_MDPMY: return "DPMY";
        case GR_MSTRSIZ: return "STRSIZ";
        case GR_MCHRPOS: return "CHRPOS";
        case GR_MWRITEJUST: return "WRITEJUST";
        case GR_MJUSTPOS: return "JUSTPOS";
        case GR_MCONDENSED: return "CONDENSED";
        case GR_MEXTENDED: return "EXTENDED";
        case GR_MXLIGHT: return "XLIGHT";
        case GR_MLIGHT: return "LIGHT";
        case GR_MXBOLD: return "XBOLD";
        case GR_MHOLLOW: return "HOLLOW";
        case GR_MRAISED: return "RAISED";
        case GR_MSETTABG: return "SETTABG";
        case GR_MRESTABG: return "RESTABG";
        case GR_MFCOLORG: return "FCOLORG";
        case GR_MBCOLORG: return "BCOLORG";
        case GR_MLOADPICT: return "LOADPICT";
        case GR_MPICTSIZX: return "PICTSIZX";
        case GR_MPICTSIZY: return "PICTSIZY";
        case GR_MPICTURE: return "PICTURE";
        case GR_MDELPICT: return "DELPICT";
        case GR_MSCROLLG: return "SCROLLG";
        case GR_MPATH: return "PATH";
        case GR_MVIEWOFFG: return "VIEWOFFG";
        case GR_MVIEWSCALE: return "VIEWSCALE";
        case GR_MSCALEX: return "SCALEX";
        case GR_MSCALEY: return "SCALEY";
        case GR_MBLOCKCOPYG: return "BLOCKCOPYG";
        case GR_MOPENWIN: return "OPENWIN";
        case GR_MBUFFER: return "BUFFER";
        case GR_MSIZBUFG: return "SIZBUFG";
        case GR_MGETSIZ: return "GETSIZ";
        case GR_MGETSIZG: return "GETSIZG";
        case GR_MSETSIZ: return "SETSIZ";
        case GR_MSETSIZG: return "SETSIZG";
        case GR_MSETPOS: return "SETPOS";
        case GR_MSETPOSG: return "SETPOSG";
        case GR_MDRAGWIN: return "DRAGWIN";
        case GR_MSCNSIZ: return "SCNSIZ";
        case GR_MSCNSIZG: return "SCNSIZG";
        case GR_MSCNCEN: return "SCNCEN";
        case GR_MSCNCENG: return "SCNCENG";
        case GR_MWINCLIENT: return "WINCLIENT";
        case GR_MWINCLIENTG: return "WINCLIENTG";
        case GR_MFRONT: return "FRONT";
        case GR_MBACK: return "BACK";
        case GR_MFRAME: return "FRAME";
        case GR_MSIZABLE: return "SIZABLE";
        case GR_MSYSBAR: return "SYSBAR";
        case GR_MMENU: return "MENU";
        case GR_MMENUENA: return "MENUENA";
        case GR_MMENUSEL: return "MENUSEL";
        case GR_MSTDMENU: return "STDMENU";
        case GR_MGETWINID: return "GETWINID";
        case GR_MFOCUS: return "FOCUS";
        case GR_MGETWIGID: return "GETWIGID";
        case GR_MKILLWIDGET: return "KILLWIDGET";
        case GR_MSELECTWIDGET: return "SELECTWIDGET";
        case GR_MENABLEWIDGET: return "ENABLEWIDGET";
        case GR_MGETWIDGETTEXT: return "GETWIDGETTEXT";
        case GR_MPUTWIDGETTEXT: return "PUTWIDGETTEXT";
        case GR_MSIZWIDGET: return "SIZWIDGET";
        case GR_MSIZWIDGETG: return "SIZWIDGETG";
        case GR_MPOSWIDGET: return "POSWIDGET";
        case GR_MPOSWIDGETG: return "POSWIDGETG";
        case GR_MBACKWIDGET: return "BACKWIDGET";
        case GR_MFRONTWIDGET: return "FRONTWIDGET";
        case GR_MFOCUSWIDGET: return "FOCUSWIDGET";
        case GR_MBUTTONSIZ: return "BUTTONSIZ";
        case GR_MBUTTONSIZG: return "BUTTONSIZG";
        case GR_MBUTTON: return "BUTTON";
        case GR_MBUTTONG: return "BUTTONG";
        case GR_MCHECKBOXSIZ: return "CHECKBOXSIZ";
        case GR_MCHECKBOXSIZG: return "CHECKBOXSIZG";
        case GR_MCHECKBOX: return "CHECKBOX";
        case GR_MCHECKBOXG: return "CHECKBOXG";
        case GR_MRADIOBUTTONSIZ: return "RADIOBUTTONSIZ";
        case GR_MRADIOBUTTONSIZG: return "RADIOBUTTONSIZG";
        case GR_MRADIOBUTTON: return "RADIOBUTTON";
        case GR_MRADIOBUTTONG: return "RADIOBUTTONG";
        case GR_MGROUPSIZG: return "GROUPSIZG";
        case GR_MGROUPSIZ: return "GROUPSIZ";
        case GR_MGROUP: return "GROUP";
        case GR_MGROUPG: return "GROUPG";
        case GR_MBACKGROUND: return "BACKGROUND";
        case GR_MBACKGROUNDG: return "BACKGROUNDG";
        case GR_MSCROLLVERTSIZG: return "SCROLLVERTSIZG";
        case GR_MSCROLLVERTSIZ: return "SCROLLVERTSIZ";
        case GR_MSCROLLVERT: return "SCROLLVERT";
        case GR_MSCROLLVERTG: return "SCROLLVERTG";
        case GR_MSCROLLHORIZSIZG: return "SCROLLHORIZSIZG";
        case GR_MSCROLLHORIZSIZ: return "SCROLLHORIZSIZ";
        case GR_MSCROLLHORIZ: return "SCROLLHORIZ";
        case GR_MSCROLLHORIZG: return "SCROLLHORIZG";
        case GR_MSCROLLPOS: return "SCROLLPOS";
        case GR_MSCROLLSIZ: return "SCROLLSIZ";
        case GR_MNUMSELBOXSIZG: return "NUMSELBOXSIZG";
        case GR_MNUMSELBOXSIZ: return "NUMSELBOXSIZ";
        case GR_MNUMSELBOX: return "NUMSELBOX";
        case GR_MNUMSELBOXG: return "NUMSELBOXG";
        case GR_MEDITBOXSIZG: return "EDITBOXSIZG";
        case GR_MEDITBOXSIZ: return "EDITBOXSIZ";
        case GR_MEDITBOX: return "EDITBOX";
        case GR_MEDITBOXG: return "EDITBOXG";
        case GR_MPROGBARSIZG: return "PROGBARSIZG";
        case GR_MPROGBARSIZ: return "PROGBARSIZ";
        case GR_MPROGBAR: return "PROGBAR";
        case GR_MPROGBARG: return "PROGBARG";
        case GR_MPROGBARPOS: return "PROGBARPOS";
        case GR_MLISTBOXSIZG: return "LISTBOXSIZG";
        case GR_MLISTBOXSIZ: return "LISTBOXSIZ";
        case GR_MLISTBOX: return "LISTBOX";
        case GR_MLISTBOXG: return "LISTBOXG";
        case GR_MDROPBOXSIZG: return "DROPBOXSIZG";
        case GR_MDROPBOXSIZ: return "DROPBOXSIZ";
        case GR_MDROPBOX: return "DROPBOX";
        case GR_MDROPBOXG: return "DROPBOXG";
        case GR_MDROPEDITBOXSIZG: return "DROPEDITBOXSIZG";
        case GR_MDROPEDITBOXSIZ: return "DROPEDITBOXSIZ";
        case GR_MDROPEDITBOX: return "DROPEDITBOX";
        case GR_MDROPEDITBOXG: return "DROPEDITBOXG";
        case GR_MSLIDEHORIZSIZG: return "SLIDEHORIZSIZG";
        case GR_MSLIDEHORIZSIZ: return "SLIDEHORIZSIZ";
        case GR_MSLIDEHORIZ: return "SLIDEHORIZ";
        case GR_MSLIDEHORIZG: return "SLIDEHORIZG";
        case GR_MSLIDEVERTSIZG: return "SLIDEVERTSIZG";
        case GR_MSLIDEVERTSIZ: return "SLIDEVERTSIZ";
        case GR_MSLIDEVERT: return "SLIDEVERT";
        case GR_MSLIDEVERTG: return "SLIDEVERTG";
        case GR_MTABBARSIZG: return "TABBARSIZG";
        case GR_MTABBARSIZ: return "TABBARSIZ";
        case GR_MTABBARCLIENTG: return "TABBARCLIENTG";
        case GR_MTABBARCLIENT: return "TABBARCLIENT";
        case GR_MTABBAR: return "TABBAR";
        case GR_MTABBARG: return "TABBARG";
        case GR_MTABSEL: return "TABSEL";
        case GR_MALERT: return "ALERT";
        case GR_MQUERYCOLOR: return "QUERYCOLOR";
        case GR_MQUERYOPEN: return "QUERYOPEN";
        case GR_MQUERYSAVE: return "QUERYSAVE";
        case GR_MQUERYFIND: return "QUERYFIND";
        case GR_MQUERYFINDREP: return "QUERYFINDREP";
        case GR_MQUERYFONT: return "QUERYFONT";
        case GR_MCAPTURENAME: return "CAPTURENAME";
        case GR_MCAPTURELABEL: return "CAPTURELABEL";
        case GR_MCAPTURE: return "CAPTURE";
        case GR_MSEAT: return "SEAT";
        case GR_MAUTOBUF: return "AUTOBUF";
        case GR_MEVENT: return "EVENT";
        case GR_MSTARTTIMEOUT: return "STARTTIMEOUT";
        case GR_MSTOPTIMEOUT: return "STOPTIMEOUT";
        case GR_MCURTIMEOUT: return "CURTIMEOUT";
        case GR_MSTARTTIMEIN: return "STARTTIMEIN";
        case GR_MSTOPTIMEIN: return "STOPTIMEIN";
        case GR_MCURTIMEIN: return "CURTIMEIN";
        case GR_MSYNTHOUT: return "SYNTHOUT";
        case GR_MSYNTHIN: return "SYNTHIN";
        case GR_MOPENSYNTHOUT: return "OPENSYNTHOUT";
        case GR_MCLOSESYNTHOUT: return "CLOSESYNTHOUT";
        case GR_MOPENSYNTHIN: return "OPENSYNTHIN";
        case GR_MCLOSESYNTHIN: return "CLOSESYNTHIN";
        case GR_MNOTEON: return "NOTEON";
        case GR_MNOTEOFF: return "NOTEOFF";
        case GR_MINSTCHANGE: return "INSTCHANGE";
        case GR_MATTACK: return "ATTACK";
        case GR_MRELEASE: return "RELEASE";
        case GR_MLEGATO: return "LEGATO";
        case GR_MPORTAMENTO: return "PORTAMENTO";
        case GR_MVIBRATO: return "VIBRATO";
        case GR_MVOLSYNTHCHAN: return "VOLSYNTHCHAN";
        case GR_MPORTTIME: return "PORTTIME";
        case GR_MBALANCE: return "BALANCE";
        case GR_MPAN: return "PAN";
        case GR_MTIMBRE: return "TIMBRE";
        case GR_MBRIGHTNESS: return "BRIGHTNESS";
        case GR_MREVERB: return "REVERB";
        case GR_MTREMULO: return "TREMULO";
        case GR_MCHORUS: return "CHORUS";
        case GR_MCELESTE: return "CELESTE";
        case GR_MPHASER: return "PHASER";
        case GR_MAFTERTOUCH: return "AFTERTOUCH";
        case GR_MPRESSURE: return "PRESSURE";
        case GR_MPITCH: return "PITCH";
        case GR_MPITCHRANGE: return "PITCHRANGE";
        case GR_MMONO: return "MONO";
        case GR_MPOLY: return "POLY";
        case GR_MLOADSYNTH: return "LOADSYNTH";
        case GR_MPLAYSYNTH: return "PLAYSYNTH";
        case GR_MDELSYNTH: return "DELSYNTH";
        case GR_MWAITSYNTH: return "WAITSYNTH";
        case GR_MWRSYNTH: return "WRSYNTH";
        case GR_MRDSYNTH: return "RDSYNTH";
        case GR_MWAVEOUT: return "WAVEOUT";
        case GR_MWAVEIN: return "WAVEIN";
        case GR_MOPENWAVEOUT: return "OPENWAVEOUT";
        case GR_MCLOSEWAVEOUT: return "CLOSEWAVEOUT";
        case GR_MLOADWAVE: return "LOADWAVE";
        case GR_MPLAYWAVE: return "PLAYWAVE";
        case GR_MDELWAVE: return "DELWAVE";
        case GR_MVOLWAVE: return "VOLWAVE";
        case GR_MWAITWAVE: return "WAITWAVE";
        case GR_MCHANWAVEOUT: return "CHANWAVEOUT";
        case GR_MRATEWAVEOUT: return "RATEWAVEOUT";
        case GR_MLENWAVEOUT: return "LENWAVEOUT";
        case GR_MSGNWAVEOUT: return "SGNWAVEOUT";
        case GR_MFLTWAVEOUT: return "FLTWAVEOUT";
        case GR_MENDWAVEOUT: return "ENDWAVEOUT";
        case GR_MWRWAVE: return "WRWAVE";
        case GR_MOPENWAVEIN: return "OPENWAVEIN";
        case GR_MCLOSEWAVEIN: return "CLOSEWAVEIN";
        case GR_MCHANWAVEIN: return "CHANWAVEIN";
        case GR_MRATEWAVEIN: return "RATEWAVEIN";
        case GR_MLENWAVEIN: return "LENWAVEIN";
        case GR_MSGNWAVEIN: return "SGNWAVEIN";
        case GR_MENDWAVEIN: return "ENDWAVEIN";
        case GR_MFLTWAVEIN: return "FLTWAVEIN";
        case GR_MRDWAVE: return "RDWAVE";
        case GR_MSYNTHOUTNAME: return "SYNTHOUTNAME";
        case GR_MSYNTHINNAME: return "SYNTHINNAME";
        case GR_MWAVEOUTNAME: return "WAVEOUTNAME";
        case GR_MWAVEINNAME: return "WAVEINNAME";
        case GR_MSETPARAMSYNTHIN: return "SETPARAMSYNTHIN";
        case GR_MSETPARAMSYNTHOUT: return "SETPARAMSYNTHOUT";
        case GR_MSETPARAMWAVEIN: return "SETPARAMWAVEIN";
        case GR_MSETPARAMWAVEOUT: return "SETPARAMWAVEOUT";
        case GR_MGETPARAMSYNTHIN: return "GETPARAMSYNTHIN";
        case GR_MGETPARAMSYNTHOUT: return "GETPARAMSYNTHOUT";
        case GR_MGETPARAMWAVEIN: return "GETPARAMWAVEIN";
        case GR_MGETPARAMWAVEOUT: return "GETPARAMWAVEOUT";
        default: return "?";
    }
}

static const char* gr_evtname(ami_long e)
{
    switch (e) {
        case ami_etchar: return "CHAR";
        case ami_etup: return "UP";
        case ami_etdown: return "DOWN";
        case ami_etleft: return "LEFT";
        case ami_etright: return "RIGHT";
        case ami_etleftw: return "LEFTW";
        case ami_etrightw: return "RIGHTW";
        case ami_ethome: return "HOME";
        case ami_ethomes: return "HOMES";
        case ami_ethomel: return "HOMEL";
        case ami_etend: return "END";
        case ami_etends: return "ENDS";
        case ami_etendl: return "ENDL";
        case ami_etscrl: return "SCRL";
        case ami_etscrr: return "SCRR";
        case ami_etscru: return "SCRU";
        case ami_etscrd: return "SCRD";
        case ami_etpagd: return "PAGD";
        case ami_etpagu: return "PAGU";
        case ami_ettab: return "TAB";
        case ami_etenter: return "ENTER";
        case ami_etinsert: return "INSERT";
        case ami_etinsertl: return "INSERTL";
        case ami_etinsertt: return "INSERTT";
        case ami_etdel: return "DEL";
        case ami_etdell: return "DELL";
        case ami_etdelcf: return "DELCF";
        case ami_etdelcb: return "DELCB";
        case ami_etcopy: return "COPY";
        case ami_etcopyl: return "COPYL";
        case ami_etcan: return "CAN";
        case ami_etstop: return "STOP";
        case ami_etcont: return "CONT";
        case ami_etprint: return "PRINT";
        case ami_etprintb: return "PRINTB";
        case ami_etprints: return "PRINTS";
        case ami_etfun: return "FUN";
        case ami_etmenu: return "MENU";
        case ami_etmouba: return "MOUBA";
        case ami_etmoubd: return "MOUBD";
        case ami_etmoumov: return "MOUMOV";
        case ami_ettim: return "TIM";
        case ami_etjoyba: return "JOYBA";
        case ami_etjoybd: return "JOYBD";
        case ami_etjoymov: return "JOYMOV";
        case ami_etresize: return "RESIZE";
        case ami_etfocus: return "FOCUS";
        case ami_etnofocus: return "NOFOCUS";
        case ami_ethover: return "HOVER";
        case ami_etnohover: return "NOHOVER";
        case ami_etterm: return "TERM";
        case ami_etframe: return "FRAME";
        case ami_etmoumovg: return "MOUMOVG";
        case ami_etredraw: return "REDRAW";
        case ami_etmin: return "MIN";
        case ami_etmax: return "MAX";
        case ami_etnorm: return "NORM";
        case ami_etmenus: return "MENUS";
        case ami_etbutton: return "BUTTON";
        case ami_etchkbox: return "CHKBOX";
        case ami_etradbut: return "RADBUT";
        case ami_etsclull: return "SCLULL";
        case ami_etscldrl: return "SCLDRL";
        case ami_etsclulp: return "SCLULP";
        case ami_etscldrp: return "SCLDRP";
        case ami_etsclpos: return "SCLPOS";
        case ami_etedtbox: return "EDTBOX";
        case ami_etnumbox: return "NUMBOX";
        case ami_etlstbox: return "LSTBOX";
        case ami_etdrpbox: return "DRPBOX";
        case ami_etdrebox: return "DREBOX";
        case ami_etsldpos: return "SLDPOS";
        case ami_ettabbar: return "TABBAR";
        case ami_etusize: return "USIZE";
        case ami_etdsize: return "DSIZE";
        default: return "?";
    }
}

#endif /* __GRAPH_REMOTE_H__ */
