********************* Hello world example compile for Petit-Ami *******************

Purpose:

This directory shows a minimal compile of the standard "hello, world" program
against Petit-Ami. The main purpose of this is to give you a typical
environment for your program, without having to account for the full Petit-Ami
build. The example uses only the include and lib directories of the tree, and
in the dynamic form the Petit-Ami stdio header in libc.

The same program builds on any of the three Petit-Ami models, in either form
of the library, chosen on the make command line:

    make                                 plain output, the static archive
    make MODEL=term                      the terminal model: output to the console
    make MODEL=graph                     the graphical model: output to a window
    make MODEL=graph LINK_TYPE=dynamic   the same, against the shared library

MODEL is plain, term or graph, and LINK_TYPE is static (the default) or dynamic.
Changing either and making again rebuilds the program; the file "options" holds
the choice the last build used. The models look like this to the program:

* plain: no terminal handler. The system, sound and network interfaces are
  available, and output goes to standard output as it would for any program.
  There are several components of Petit-Ami that don't rely on terminal or
  graphical output, so there is good reason to compile plain programs.
* term: the terminal model takes over the console as the program starts, and
  "hello, world" appears on a cleared screen.
* graph: the graphical model opens a window as the program starts, and
  "hello, world" appears in it. The window stays until it is closed.

Prerequisites:

The dependent libraries must be present in your system, OpenSSL, ALSA,
fluidsynth, and for the graphical model the libraries of the display it was
built for. The Petit-Ami library this example links is made by the build at
the root: ../lib/libami_<model>.a for the static form, ../lib/petit_ami_<model>.so
for the dynamic form. The makefile here makes it there if it is not already
made, so a plain "make" in this directory works in a tree that has never been
built.

The root compiles its objects for one link type or the other, so a tree built
in one form must be cleaned at the root (make clean there) before this example
is built in the other.

The makefile finds the root from its own path rather than from the directory
make was started in, so it builds the same however it is reached, and the
program it makes runs from any directory: in the dynamic form it carries the
path of the lib directory, so the shared library is found wherever it runs
from.

How the makefile is put together:

* The system libraries every model wants come first: sound, SSL, the C++
  runtime (the Petit-Ami core carries the C++ binding), math and threads. The
  sound and network parts of the library are drawn in only by a program that
  calls them.
* The graphical model also wants the libraries of the display backend the
  graphical library was built with. The root build records that in
  lib/graph_backend (wayland, x11 or fb), and the makefile reads it to choose.
* The terminal and graphical models take over standard output as the program
  starts, so even a program that calls nothing in them wants them linked. A
  library is drawn in only by the calls made to it, so the makefile asks the
  linker for one of the model's functions by name (-u ami_maxx). The plain
  model needs nothing of the kind.
* Sound cannot live in a shared library (an ALSA bug), so the dynamic form
  links ../lib/sound.o directly beside the shared library.
* The two forms meet the program's printf differently. The archive carries
  Petit-Ami's stdio under the standard names, so the program compiles against
  the system stdio.h and its printf lands in the model when it is linked. The
  shared library keeps Petit-Ami's stdio under its own names beside the
  system one, so the dynamic form compiles the program against Petit-Ami's
  stdio header, ../libc/stdio.h, with STDIO_BYPASS defined, which gives
  printf that name. Without it the program's output goes to the system
  stdout and never reaches the terminal or the window.
