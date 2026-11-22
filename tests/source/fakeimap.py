#!/usr/bin/env python3
"""A small IMAP server that serves a fixed set of messages, for testing a
client without an account. Speaks just enough: LOGIN, LIST, EXAMINE,
FETCH (UID), UID FETCH BODY.PEEK[], LOGOUT. TLS with a self signed cert."""

import base64, socket, ssl, sys, threading

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9993
CERT = sys.argv[2] if len(sys.argv) > 2 else "fake.pem"

def msg_plain():
    return ("Return-Path: <reddit@redditmail.com>\r\n"
            "From: Reddit <noreply@redditmail.com>\r\n"
            "To: someone@gmail.com\r\n"
            "Subject: \"Anthropic, get it together.\"\r\n"
            "Date: Fri, 1 Aug 2025 13:06:22 -0700\r\n"
            "Message-ID: <1@redditmail.com>\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "\r\n"
            "r/Anthropic: Anthropic, get it together. After months of\r\n"
            "waiting the thing still does not do what it says on the tin.\r\n"
            "\r\n"
            "Reply to this email to comment.\r\n")

def msg_mime():
    plain = ("Eighteen years married, so I have some standing here. When I\r\n"
             "was 18 I had just returned from 2nd shift in a nursing home.\r\n")
    html = ("<html><head><style>p{color:red}</style></head><body>"
            "<p>Eighteen years married, so I have some standing here.</p>"
            "<p>When I was 18 I had just returned from 2nd shift.</p>"
            "</body></html>")
    subj = base64.b64encode("When I was 18 I had just returned from 2nd shift"
                            .encode()).decode()
    return ("From: \"Quora Digest\" <digest-noreply@quora.com>\r\n"
            "To: someone@gmail.com\r\n"
            "Subject: =?UTF-8?B?" + subj + "?=\r\n"
            "Date: Fri, 1 Aug 2025 07:35:02 -0700\r\n"
            "MIME-Version: 1.0\r\n"
            "Content-Type: multipart/alternative; boundary=\"XX99\"\r\n"
            "\r\n"
            "--XX99\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Transfer-Encoding: quoted-printable\r\n"
            "\r\n" + plain.replace("=", "=3D") +
            "--XX99\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "\r\n" + html + "\r\n"
            "--XX99--\r\n")

def msg_b64():
    body = ("Your order has shipped. There is still time to order more of\r\n"
            "the same, which is the whole point of telling you.\r\n")
    return ("From: AliExpress <transaction@notice.aliexpress.com>\r\n"
            "To: someone@gmail.com\r\n"
            "Subject: Great finds await your return\r\n"
            "Date: Thu, 31 Jul 2025 09:12:00 -0700\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Transfer-Encoding: base64\r\n"
            "\r\n" + base64.b64encode(body.encode()).decode() + "\r\n")

def msg_html():
    return ("From: The Economist <newsletters@e.economist.com>\r\n"
            "To: someone@gmail.com\r\n"
            "Subject: Why over-55s are the new problem generation\r\n"
            "Date: Wed, 30 Jul 2025 04:00:00 -0700\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "\r\n"
            "<html><body><script>var x=1;</script>"
            "<div>Also: How the boomers screwed the young, &amp; what to "
            "do about it.</div><br><div>Read on.</div></body></html>\r\n")

def msg_from():
    """A message whose body holds a line that looks like a separator, to
       prove the mbox quoting works."""
    return ("From: Test <test@example.com>\r\n"
            "To: someone@gmail.com\r\n"
            "Subject: A body that looks like a separator\r\n"
            "Date: Tue, 29 Jul 2025 12:00:00 -0700\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "The next line is the trap:\r\n"
            "From nobody Mon Jan 1 00:00:00 2001\r\n"
            "and this line must stay in the same message.\r\n")

FOLDERS = {
    "INBOX": [msg_plain(), msg_mime(), msg_b64(), msg_html(), msg_from()],
    "[Gmail]/Sent Mail": [msg_plain()],
    "[Gmail]/Spam": [],
    "quora": [msg_mime()],
}

def serve(conn):
    f = conn.makefile("rwb")
    def send(s):
        f.write(s.encode()); f.flush()
    send("* OK fake IMAP ready\r\n")
    sel = None
    while True:
        line = f.readline()
        if not line: break
        line = line.decode("utf8", "replace").rstrip("\r\n")
        parts = line.split(" ", 1)
        if len(parts) < 2: continue
        tag, cmd = parts[0], parts[1]
        up = cmd.upper()
        if up.startswith("LOGIN"):
            send("%s OK LOGIN done\r\n" % tag)
        elif up.startswith("LIST"):
            send('* LIST (\\HasChildren \\Noselect) "/" "[Gmail]"\r\n')
            for name in FOLDERS:
                send('* LIST (\\HasNoChildren) "/" "%s"\r\n' % name)
            send("%s OK LIST done\r\n" % tag)
        elif up.startswith("EXAMINE") or up.startswith("SELECT"):
            name = cmd.split(" ", 1)[1].strip().strip('"')
            sel = FOLDERS.get(name, [])
            # As Gmail answers, in Gmail's order. RECENT after EXISTS
            # matters: a client that parses EXISTS loosely will take the
            # RECENT count for it and think the folder is empty.
            send("* FLAGS (\\Answered \\Flagged \\Draft \\Deleted \\Seen)\r\n")
            send("* OK [PERMANENTFLAGS ()] Flags permitted.\r\n")
            send("* OK [UIDVALIDITY 42] UIDs valid.\r\n")
            send("* %d EXISTS\r\n" % len(sel))
            send("* 0 RECENT\r\n")
            send("* OK [UIDNEXT %d] Predicted next UID.\r\n" % (len(sel)+1))
            send("* OK [HIGHESTMODSEQ 6020170]\r\n")
            send("%s OK [READ-ONLY] EXAMINE done\r\n" % tag)
        elif up.startswith("UID FETCH"):
            uid = int(cmd.split()[2])
            if sel is None or uid < 1 or uid > len(sel):
                send("%s NO no such message\r\n" % tag)
                continue
            body = sel[uid-1]
            send("* %d FETCH (UID %d BODY[] {%d}\r\n" % (uid, uid, len(body)))
            send(body)
            send(")\r\n")
            send("%s OK FETCH done\r\n" % tag)
        elif up.startswith("FETCH"):
            rng = cmd.split()[1]
            lo, hi = (rng.split(":") + [rng])[:2]
            lo, hi = int(lo), len(sel or []) if hi == "*" else int(hi)
            for n in range(lo, hi+1):
                send("* %d FETCH (UID %d)\r\n" % (n, n))
            send("%s OK FETCH done\r\n" % tag)
        elif up.startswith("LOGOUT"):
            send("* BYE\r\n%s OK LOGOUT done\r\n" % tag)
            break
        elif up.startswith("CAPABILITY"):
            send("* CAPABILITY IMAP4rev1\r\n%s OK done\r\n" % tag)
        else:
            send("%s BAD unknown\r\n" % tag)
    try: conn.close()
    except OSError: pass

ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain(CERT)
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", PORT))
srv.listen(5)
print("fake imap on %d" % PORT, flush=True)
while True:
    c, _ = srv.accept()
    try:
        c = ctx.wrap_socket(c, server_side=True)
    except ssl.SSLError as e:
        print("tls failed:", e, flush=True)
        continue
    threading.Thread(target=serve, args=(c,), daemon=True).start()
