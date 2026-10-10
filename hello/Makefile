#
# Example makefile for the hello world program, built on Petit-Ami
#
# The same program builds on any of the three Petit-Ami models, against
# either form of the library:
#
#   MODEL=plain      no terminal handler: system, sound and network only
#   MODEL=term       the terminal model: output goes to the console
#   MODEL=graph      the graphical model: output goes to a window
#
#   LINK_TYPE=static     the archive ../lib/libami_<model>.a (the default)
#   LINK_TYPE=dynamic    the shared library ../lib/petit_ami_<model>.so
#
# Either is given on the command line, as in
#
#   make MODEL=graph LINK_TYPE=dynamic
#
# or set here. Changing them rebuilds the program. The example uses only
# the include and lib directories of the Petit-Ami tree, and in the dynamic
# form its stdio header in libc.
#
MODEL ?= plain
LINK_TYPE ?= static

#
# Where Petit-Ami stands.
#
# Taken from this makefile's own path, not from the directory make was started
# in, so the example builds the same from here, from the root with
# make -C hello, or from anywhere with make -f.
#
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)

#
# The system libraries every model wants.
#
# The C++ runtime is here because the Petit-Ami core carries the C++ binding
# with it, and libc is not named at all: the program takes the system one.
# The sound and network parts of the library are drawn in only by a program
# that calls them, so a program like this one that calls neither pays for
# neither.
#
LIBS = -lasound -lfluidsynth -lssl -lcrypto -lstdc++ -lm -lpthread

#
# The system libraries the graphical model draws with. They depend on the
# backend the graphical library was built with, which the root build
# records in lib/graph_backend: wayland, x11 or fb.
#
BACKLIBS_wayland = -lwayland-client -lwayland-cursor -lxkbcommon \
                   -lfreetype -lfontconfig
BACKLIBS_x11 = -lX11 -lXtst -lfreetype -lfontconfig
BACKLIBS_fb = -lfreetype -lfontconfig -lpng -lz

# read when the link runs, after the library is made
GRAPHLIBS = $(BACKLIBS_$(shell cat $(ROOT)/lib/graph_backend 2> /dev/null))

ifeq ($(MODEL),graph)
    LIBS += $(GRAPHLIBS)
endif

#
# Keeping the model in the link.
#
# The terminal and graphical models take over stdout as the program starts,
# so a program that calls nothing in them, like this one, still wants them
# linked. Named only by its calls, the library would be left out, so the
# link asks for one of the model's functions by name. The plain model has
# no such takeover, and needs nothing.
#
ifneq ($(MODEL),plain)
    KEEP = -Wl,-u,ami_maxx
endif

#
# The Petit-Ami library, in the form chosen.
#
# Sound cannot live in a shared library (an ALSA bug), so in the dynamic form
# its object links directly beside the library, and the program carries the
# path of the lib directory so that it runs from anywhere. The sound object
# stands before the shared library: the linker keeps a shared library only
# for references seen before it, and the sound object's are the first.
#
# The two forms meet the program's own printf differently. The archive
# carries Petit-Ami's stdio under the standard names, so the program is
# compiled against the system header and its printf lands in the model at
# link. The shared library keeps Petit-Ami's stdio under its own names
# beside the system one, so the program is compiled against Petit-Ami's
# stdio header (../libc/stdio.h), which gives printf that name.
#
ifeq ($(LINK_TYPE),static)
    AMILIB = $(ROOT)/lib/libami_$(MODEL).a
    AMILINK = $(AMILIB)
    AMICFLAGS =
else
    AMILIB = $(ROOT)/lib/sound.o $(ROOT)/lib/petit_ami_$(MODEL).so
    AMILINK = $(AMILIB) -Wl,-rpath,$(ROOT)/lib
    AMICFLAGS = -I$(ROOT)/libc -DSTDIO_BYPASS
endif

hello: hello.c Makefile options $(AMILIB)
	gcc -g3 -I$(ROOT)/include $(AMICFLAGS) hello.c $(KEEP) $(AMILINK) $(LIBS) -o hello

#
# The options the last build used. The file changes only when they do, so
# a build with other options remakes the program and one with the same
# leaves it alone.
#
options: FORCE
	@[ "`cat options 2> /dev/null`" = "$(MODEL) $(LINK_TYPE)" ] || \
	    echo $(MODEL) $(LINK_TYPE) > options

FORCE:

#
# What the example borrows from the main build, made there when the tree has
# not been built yet. The link type goes along, since the root compiles
# its objects for one or the other: a tree built in one form must be
# cleaned at the root before it is built in the other.
#
$(ROOT)/lib/%:
	$(MAKE) -C $(ROOT) lib/$* LINK_TYPE=$(LINK_TYPE)

clean:
	rm -f hello options
