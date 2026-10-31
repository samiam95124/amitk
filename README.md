# Ami !

**One program source, presented wherever you run it.** The same
"hello, world" -- the standard C program, unchanged -- on a command
line, on a terminal, and in a graphical window:

| Command line | Terminal | Graphical |
|:---:|:---:|:---:|
| ![hello on the command line](doc/images/hello_cmdline.png) | ![hello on a terminal](doc/images/hello_console.png) | ![hello in a window](doc/images/hello_graphical.png) |

And the same idea carried to a full application. This is one mail
client -- one core holding the store, the fetch engine and the
protocols -- with front ends that differ only in how they draw. In
pixels; in character cells on a stock terminal, windows and widgets
managed by Ami's own character-mode window manager; and the character
front end linked against the graphical library, which speaks the whole
terminal API onto a window of its own:

**mail** -- the graphical front end:

![the mail client, graphical](doc/images/mail_graphical.png)

**mailc** -- the same mail in character cells, running in a plain
terminal, with menus, windows and widgets from the character-mode
window manager:

![the mail client on a terminal](doc/images/mail_terminal.png)

**mailcg** -- the character front end, unchanged, compiled against the
graphical library:

![the character mail client on a graphical window](doc/images/mail_charwin.png)

And the whole of it in motion. Breakout, run nine ways: in characters
and in pixels, on this machine, over ssh, on an iPhone, over Ami's own remote
protocol, over waypipe, and on Mac OS X and Windows -- the same program
each time. 

https://github.com/user-attachments/assets/bbe52100-d5a4-46cc-a86e-0b3983cd6277

Note that not all transports carry sound, so some of the demos in the video
do not carry sound. Note however that the Ami remote transport does carry
sound ! In fact it is the only transport out of Wayland, X11 and WayPipe that does.

Click to play on Youtube:

[![Ami: breakout in Linux, Windows and Mac OS X, in multiple modes](https://img.youtube.com/vi/BLPet3lhYbM/maxresdefault.jpg)](https://youtu.be/BLPet3lhYbM)

And an instrument on the bench. This is a remote control for a Rigol
MSO1104Z oscilloscope, drawn as the face of the scope, with the scope's
own screen fetched over USB and the keys and knobs working the real ones
([mso1104](https://github.com/samiam95124/mso1104)):

![the MSO1104 front panel](doc/images/mso1104_panel.png)

![the scope itself, with the panel on the monitor behind it](doc/images/mso1104_scope.jpg)

---

The AMI tk is a toolkit based on the idea that carefully crafted display
paradigms can carry forward. Ami starts with the serial display paradigm
and carries that into a terminal oriented paradigm, then to a fully
graphical and windowed paradigm.

Web page for Ami !

https://samiam95124.github.io/amitk

To fetch and build it (the tree carries submodules, so take them along):

    git clone --recurse-submodules https://github.com/samiam95124/amitk.git
    cd amitk
    . setpath
    ./configure
    make

The full procedure is in [INSTALL](INSTALL).

Please see the following documents to get started in Petit-Ami

    doc/petit_ami.docx  The "grand unified" document of what it is, how to
                        install it, etc.
    INSTALL             The basic install procedure.

The project is presented as complete within a single tree. The format is
along the lines of GNU projects, IE, with a configure, Makefile, README, etc.
