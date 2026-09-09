################################################################################
#                                                                              #
#               Makefile for Petit Ami and associated programs                 #
#                                                                              #
################################################################################
#
# Structure of makefile
#
# There are six sections to the makefile:
#
# 1. Establishing macros to make products according to OS and type of build.
#
# 2. Building individual components.
#
# 3. Build libraries.
#
# 4. Building target programs (tests, demos, utilities, etc.)
#
# 5. Define build targets.
#
# 6. Define clean targets.
#
# The macros that are constructed are:
#
# CC		Contains the compiler definition in use.
#
# CFLAGS	Contains the compiler/linker flags to build programs.
#
# CFLAGSCPP Contains the compiler/linker flags to build C++ programs.
#
# PLIBS		Contains the libraries and flags required to build plain (no display
#           model) programs.
# 
# CLIBS		Contains the libraries and flags required to build console model
#           programs.
#
# CLIBSCPP  Contains the libraries and flags required to build console model
#           programs for C++.
#
# GLIBS		Contains the libraries and flags required to build graphical model
#           programs.
#
# PLIBSD	Contains dependencies only for plain (no display model) programs.
#
# CLIBSD    Contains dependencies only for console model programs.
#
# CLIBSCPPD Contains dependencies only for console model programs in C++.
#
# GLIBSD    Contains dependencies only for graphical model programs.
#
# The depenencies contain the collected library in use (.so or .a) with the
# correct extension.
#
# The ending of the program name gives the display model the program was 
# constructed for, one of:
#
# <none>	No display model, ie., serial console.
#
# c			Console model.
#
# g			Graphical model.
#
# In all cases, the same source file is used to generate all products, if 
# multiple output products exist (indeed, that is the point of Petit-Ami).
# Not all products can be made from a given source file. For example, a 
# graphical program cannot be also ported to console or serial mode.
#
# Note that the "c" or "console" postfix is often dropped, since only the "g"
# or "graphical" ending is necessary to differentiate the products.
#
# Note that "g" or "graphical" may denote a console program that can be compiled
# in either console or graphical mode, or it may denote a program specifically
# designed for graphical mode.
#
# Note that most of the complexity of this make is to create the libraries for
# specific implementations. There are sample makefiles elsewhere that are much
# simpler, because they use prebuilt libraries. Please see those examples.
#

#
# Version
#
VERMAJOR=0
VERMINOR=1

################################################################################
#
# Establish build macros
#
################################################################################

#
# Set OSTYPE according to operating system
#
# If the OS environment variable is set, we use it, otherwise we assume it is
# A Unix variant and has uname. This works for linux and Mac OS X.
#
ifeq ($(OS),Windows_NT)
    OSTYPE=Windows_NT
else
	OSTYPE=$(shell uname)
endif

#
# Use clang or use gcc
#
# Note some systems only can use one or the other.
#
ifndef CLANG
	CLANG=0
endif

#
# Where do stdio definitions and overrides come from?
#
ifndef STDIO_SOURCE
    ifeq ($(OSTYPE),Windows_NT)

        #
        # stdio is a local stdio implementation with overrides.
        # Note it is not a complete libc. It works because in dynamic linking a module
        # can effectively replace a later module of the same names. It does not work
        # for static linking or if other modules in chain bypass posix standards.
        # 
        # Windows builds must use stdio since they don't use glibc.
        #
        STDIO_SOURCE=stdio

    else ifeq ($(OSTYPE),Darwin)

        #
        # Mac OS x builds must use stdio since they don't use glibc.
        #
        STDIO_SOURCE=stdio

    else ifeq ($(OSTYPE),FreeBSD)

        #
        # BSD builds must use stdio since they don't use glibc.
        #
        STDIO_SOURCE=stdio

    else

        #
        # Linux
        #
        # The local stdio with overrides (STDIO_BYPASS) is the standard.
        # A patched glibc preceded it, and was dropped: too complex, too
        # version specific.
        #
        STDIO_SOURCE=stdio

    endif
endif

#
# Link image statically or dynamically?
#
ifndef LINK_TYPE

    ifeq ($(OSTYPE),Windows_NT)
    
        #
        # Windows
        #
        # Windows is always static. .dlls are added during load time.
        #
        LINK_TYPE=static

    else ifeq ($(OSTYPE),Darwin)
    
        #
        # Mac OS X is static
        #
        LINK_TYPE=static
        
    else ifeq ($(OSTYPE),FreeBSD)
    
        #
        # FreeBSD is static
        #
        LINK_TYPE=static

    else
    
        #
        # Linux
        #
        # Static, like the other platforms: each level links as one
        # combined object with everything inside (see the .o rules with
        # the Linux libraries below). LINK_TYPE=dynamic still selects
        # the .so configuration.
        #
        LINK_TYPE=static
        
    endif

endif

#
# Which graphics backend serves the graphical model? (Linux static only:
# the backend is swapped by link option.) Defaults to the running
# session: a Wayland display present selects the native Wayland backend,
# else Xlib. Override on the command line with GRAPHICS_BACKEND=x11,
# GRAPHICS_BACKEND=wayland or GRAPHICS_BACKEND=fb (the Linux frame
# buffer: the stack framebuffer <- graphics <- windowg <- widgets, run
# from a text console). The explicit graphics_testw/widget_testw
# targets build the Wayland backend regardless. The terminal model is
# untouched by this knob: terminal output is ANSI escapes, which the
# standalone console interprets natively.
#
ifndef GRAPHICS_BACKEND

    ifneq ($(WAYLAND_DISPLAY),)

        GRAPHICS_BACKEND=wayland

    else

        GRAPHICS_BACKEND=x11

    endif

endif
# the long spelling is accepted
ifeq ($(GRAPHICS_BACKEND),framebuffer)

    GRAPHICS_BACKEND=fb

endif
# the dynamic link has no Wayland or frame buffer library; the X
# backend serves it
ifneq ($(LINK_TYPE),static)

    GRAPHICS_BACKEND=x11

endif
# the graphical archive name the backend selects
ifeq ($(GRAPHICS_BACKEND),wayland)

    GRAPHLIB=graphw

else ifeq ($(GRAPHICS_BACKEND),fb)

    GRAPHLIB=graphfb

else

    GRAPHLIB=graph

endif

#
# Does terminal model get window management?
#
ifndef USEWINDOWC

	#
	# Default is windowc disabled on terminal mode
	#
	USEWINDOWC=0

endif

#
# Set windowc object
#
ifeq ($(USEWINDOWC),1)

	WINDOWC=portable/windowc.o

else

	WINDOWC=

endif

ifeq ($(OSTYPE),Windows_NT)

	ifeq ($(CLANG),1)

		CC=clang
		CPP=clang++

	else

		CC=gcc
		CPP=g++

	endif
	CFLAGS=-g3 -Iinclude

else ifeq ($(OSTYPE),Darwin)

	CC=clang
	CPP=clang++
	# Use Homebrew OpenSSL by default on macOS; override with USE_LIBRESSL=1 to use Homebrew LibreSSL
	ifdef USE_LIBRESSL
	    SSL_PREFIX=$(shell /opt/homebrew/bin/brew --prefix libressl)
	    SSL_CFLAGS=-I$(SSL_PREFIX)/include -DUSE_LIBRESSL
	    SSL_LIBS=$(SSL_PREFIX)/lib/libssl.dylib $(SSL_PREFIX)/lib/libcrypto.dylib
	else
	    SSL_PREFIX=$(shell /opt/homebrew/bin/brew --prefix openssl@3)
	    SSL_CFLAGS=-I$(SSL_PREFIX)/include -DUSE_OPENSSL
	    SSL_LIBS=$(SSL_PREFIX)/lib/libssl.dylib $(SSL_PREFIX)/lib/libcrypto.dylib
	endif
	FT_PREFIX=$(shell /opt/homebrew/bin/brew --prefix freetype)
	CFLAGS=-g3 -Iinclude -ffp-contract=off $(SSL_CFLAGS) -I$(FT_PREFIX)/include/freetype2 -I/opt/X11/include

else ifeq ($(OSTYPE),FreeBSD)

	CC=clang
	CPP=clang++
	CFLAGS=-g3 -Iinclude -I/usr/local/include/freetype2 -fcommon

else

	# linux
	ifeq ($(CLANG),1)

		CC=clang
		CPP=clang++

	else

		CC=gcc
		CPP=g++

	endif
	CFLAGS=-g3 -Iinclude -fPIC

endif

#
# Add flags by OS
#
ifeq ($(OSTYPE),Windows_NT)

	# Windows, nothing
	
else ifeq ($(OSTYPE),Darwin)

    # Mac OS X, nothing

else ifeq ($(OSTYPE),FreeBSD)

    # BSD, nothing

else

    #
    # Linux
    #
    # FreeType/fontconfig for font rendering
    CFLAGS+=$(shell pkg-config --cflags freetype2 fontconfig)
    
endif

#
# Set library dependencies. The static archives carry the lib prefix
# (libami_<model>.a), so they also serve -lami_<model>; the shared
# libraries keep their historical names.
#
ifeq ($(LINK_TYPE),static)
    LIBEXT = .a
    LIBPFX = lib/libami_
else
    LIBEXT = .so
    LIBPFX = lib/petit_ami_
endif

#
# Select where stdio.h comes from
#
ifeq ($(STDIO_SOURCE),stdio)
    #
    # In local link, we need to get stdio.h from local directory
    #
    CFLAGS +=-Ilibc
    ifeq ($(OSTYPE),FreeBSD)
        # FreeBSD keeps the coined-namespace bypass: its libc is not
        # glibc, and the override scheme below is built on glibc facts.
        CFLAGS += -DSTDIO_BYPASS
    else ifeq ($(OSTYPE),Windows_NT)
        # Windows keeps the coined namespace too. Everything links
        # statically there, prebuilt archives included, so in override
        # mode a third party archive's fread or fgets bound petit ami's
        # stdio while its FILEs came from the C runtime (issue #133).
        # Under stdio_ names petit ami's stdio is the program's alone,
        # and every archive runs wholly on the runtime's. The two modes
        # and the rule that goes with either are described at the head
        # of libc/stdio.c.
        CFLAGS += -DSTDIO_BYPASS
    else ifeq ($(OSTYPE),Darwin)
        # Mac OS X overrides at link time, as always
    else ifeq ($(LINK_TYPE),dynamic)
        # The dynamic configuration keeps the STDIO_BYPASS coined
        # namespace: petit ami's stdio lives beside glibc's under
        # stdio_ names, paired with programs through the local header.
        CFLAGS += -DSTDIO_BYPASS
    else
        # The static configuration runs stdio in override mode with
        # hidden dynamic visibility (see the linux/stdio.o rule). The
        # program and the petit ami archives resolve the real stdio
        # names to petit ami's stdio at static link -- a program needs
        # no special headers, printf lands in the window -- while the
        # symbols stay out of the dynamic table, so every shared
        # library (X, ALSA, and glibc's own NSS lookups, whose FILEs
        # glibc processes internally) binds glibc's stdio and runs
        # wholly on glibc FILEs. The two stdio worlds split at the
        # dynamic boundary, which no FILE crosses.
        # tools/stdioaudit lists a binary's stdio surface if a new
        # system needs checking. Switching LINK_TYPE requires a clean
        # build: the modes compile stdio call sites differently.
        STDIOVIS = -fvisibility=hidden
    endif
endif

#
# modify compile flags for static operation
#
ifeq ($(LINK_TYPE),static)

    ifeq ($(OSTYPE),Windows_NT)

	    # Windows
	    CFLAGS += -static
	
    else ifeq ($(OSTYPE),Darwin)

        # Mac OS X, nothing
    
    else ifeq ($(OSTYPE),FreeBSD)

        # BSD, nothing

    else

        # Linux: nothing. The static configuration is mostly static:
        # Petit-Ami's code links from the archives into the executable,
        # and the system libraries -- glibc, ALSA, fluidsynth, OpenSSL,
        # X -- stay shared. ALSA reaches PulseAudio through its shared
        # bridge module, so sound mixes with the desktop as usual. A
        # fully static binary is not supported: ALSA and PulseAudio do
        # not support static operation, and that is theirs to change.
        
    endif
    
endif

#
# No reason at present for CPP to be different than C
#
CFLAGSCPP = $(CFLAGS) -Ihpp

#
# Specify object file for libc
#
ifeq ($(OSTYPE),Windows_NT)

	# Nothing, libc is linked in overall lib
	
else ifeq ($(OSTYPE),Darwin)

    # Nothing, libc is linked in overall lib
    
else ifeq ($(OSTYPE),FreeBSD)

    # Nothing, libc is linked in overall lib

else

	# Linux: nothing, libc is linked in overall lib. (The patched glibc
	# configuration that was dropped linked bin/libc here, first.)
    
endif

#
# Specify stdio for Linux libraries
#
ifeq ($(STDIO_SOURCE),stdio)

	LINUXSTDIO = linux/stdio.o
	
else
	
	LINUXSTDIO =
	
endif

#
# X and png link tail for programs that use them directly (the test
# harness's screen capture).
#
ifeq ($(OSTYPE),Windows_NT)
# Windows screen capture uses GDI, not X11, so libpng/zlib are required but
# libX11 is not linked.
XLIBS = -lpng -lz
else
XLIBS = -lX11 -lpng -lz
endif

#
# Collected libraries
#

#
# Plain (no terminal handler)
# This option exists to drop the terminal handler, which should not be
# required for most code.
#
# Note that this is more important for Linux than Windows, because Windows
# console is "transparent", or unchanging depending on mode.
#
# Note there is no statically linked sound at the moment, since we don't have
# an absolute version of fluidsynth.
#
# The libraries are wrapped as defines as:
#
# LIBS		Full Petit-Ami libraries including console or graphics.
# PLIBS     Petit-Ami libraries without console or graphics.
#
ifeq ($(LINK_TYPE),static)
    ifeq ($(OSTYPE),Darwin)
    	PLIBS += lib/libami_plain.a
    else ifeq ($(OSTYPE),Windows_NT)
    	PLIBS += -Wl,--whole-archive lib/libami_plain.a -Wl,--no-whole-archive
    else ifeq ($(OSTYPE),FreeBSD)
    	PLIBS += -Wl,--whole-archive lib/libami_plain.a -Wl,--no-whole-archive
    else
        # Linux: the model's bundle archive. Sound and network are bundle
        # members, pulled only when the program uses them.
    	PLIBS += lib/libami_plain.a
    endif
else
    PLIBS += lib/petit_ami_plain.so
endif

#
# terminal handler libraries
#

#
# Terminal model API
#
ifeq ($(LINK_TYPE),static)
    ifeq ($(OSTYPE),Darwin)
    	CLIBS += lib/libami_term.a
    else ifeq ($(OSTYPE),Windows_NT)
    	CLIBS += -Wl,--whole-archive lib/libami_term.a -Wl,--no-whole-archive
    else ifeq ($(OSTYPE),FreeBSD)
    	CLIBS += -Wl,--whole-archive lib/libami_term.a -Wl,--no-whole-archive
    else
        # Linux: the model's bundle archive. keeper forces the core in for
        # programs that make no terminal calls of their own; sound and
        # network members pull only when used.
    	CLIBS += stub/keeper.o lib/libami_term.a
    endif
else
    CLIBS += stub/keeper.o lib/petit_ami_term.so
endif

# the C++ wrapper is already packaged inside the library (term_core.o)
CLIBSCPP = $(CLIBS)

#
# Graphical model API
#
ifeq ($(LINK_TYPE),static)
    ifeq ($(OSTYPE),Darwin)
    	GLIBS += -Wl,-force_load,lib/libami_graph.a -Wl,-e,_pa_entry
    else ifeq ($(OSTYPE),Windows_NT)
    	GLIBS += -Wl,--whole-archive lib/libami_graph.a -Wl,--no-whole-archive
    else ifeq ($(OSTYPE),FreeBSD)
    	GLIBS += -Wl,--whole-archive lib/libami_graph.a -Wl,--no-whole-archive
    else
        # Linux: the model's bundle archive, as for the terminal model.
        # GRAPHLIB carries the backend choice: graph (Xlib) or graphw
        # (Wayland)
    	GLIBS += stub/keeper.o lib/libami_$(GRAPHLIB).a
    endif
else
    GLIBS += stub/keeper.o lib/petit_ami_graph.so
endif

#
# The Wayland graphical model: identical contract, the backend swapped in by
# link option (Linux static only)
#
GLIBSW = stub/keeper.o lib/libami_graphw.a
GLIBSWD = lib/libami_graphw.a stub/keeper.o

#
# Create dependency macros
#
PLIBSD += $(LIBPFX)plain$(LIBEXT)
CLIBSD += $(LIBPFX)term$(LIBEXT) stub/keeper.o
GLIBSD += $(LIBPFX)$(GRAPHLIB)$(LIBEXT) stub/keeper.o

CLIBSCPPD = $(CLIBSD)
#
# add external packages
#
ifeq ($(OSTYPE),Windows_NT)

    #
    # Windows
    #
    PLIBS += -lwinmm -lssl -lcrypto -lws2_32 -lcrypt32
    CLIBS += -lwinmm -lgdi32 -lssl -lcrypto -lws2_32 -lcrypt32
    GLIBS += -lwinmm -lgdi32 -lcomdlg32 -lssl -lcrypto -lws2_32 -lcrypt32

else ifeq ($(OSTYPE),Darwin)

    #
    # Mac OS X
    #
    PLIBS += $(SSL_LIBS) \
             -framework CoreFoundation \
             -framework CoreGraphics \
             -framework ImageIO \
             -framework CoreMIDI \
             -framework AudioToolbox
    CLIBS += $(SSL_LIBS) \
             -framework CoreFoundation \
             -framework CoreGraphics \
             -framework ImageIO \
             -framework CoreMIDI \
             -framework AudioToolbox
    GLIBS += $(SSL_LIBS) \
             -framework Cocoa \
             -framework CoreGraphics \
             -framework CoreText \
             -framework CoreFoundation \
             -framework ImageIO \
             -framework QuartzCore \
             -framework CoreMIDI \
             -framework AudioToolbox \
             -framework IOKit

else ifeq ($(OSTYPE),FreeBSD)

    #
    # BSD
    #
	PLIBS += -L/usr/local/lib -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
	CLIBS += -L/usr/local/lib -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
	GLIBS += -L/usr/local/lib -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto \
	         -lX11 -lfreetype -lfontconfig
	PLIBSD +=
    CLIBSD +=
	GLIBSD +=

else

    #
    # Linux
    #
    ifeq ($(LINK_TYPE),static)

	# The archives carry Petit-Ami's code; the system libraries stay
	# shared. The sound and network tails cost nothing when unused:
	# with the members not pulled, as-needed linking drops them.
	# stdc++: the plain core carries the sound C++ wrapper
	PLIBS += -lasound -lfluidsynth -lssl -lcrypto -lstdc++ -lm -lpthread
	CLIBS += -lasound -lfluidsynth -lssl -lcrypto -lstdc++ -lm -lpthread
    ifeq ($(GRAPHICS_BACKEND),wayland)
	GLIBS += -lasound -lfluidsynth -lssl -lcrypto -lstdc++ \
	         -lwayland-client -lwayland-cursor -lxkbcommon \
	         -lfreetype -lfontconfig -lm -lpthread
    else ifeq ($(GRAPHICS_BACKEND),fb)
	GLIBS += -lasound -lfluidsynth -lssl -lcrypto -lstdc++ \
	         -lfreetype -lfontconfig -lpng -lz -lm -lpthread
    else
	GLIBS += -lasound -lfluidsynth -lssl -lcrypto -lstdc++ -lX11 \
	         -lfreetype -lfontconfig -lm -lpthread
    endif

    else

	# Sound cannot live in an .so (an ALSA bug), so its objects link
	# directly in the dynamic configuration.
	PLIBS += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o \
	         -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
	CLIBS += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o \
	         -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto
	GLIBS += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o \
	         -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto -lX11 \
	         -lfreetype -lfontconfig
	PLIBSD += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o
	CLIBSD += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o
	GLIBSD += linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o

    endif
    
endif

################################################################################
#
# Build targets
#
################################################################################

#
# Make all executables
#        
ifeq ($(OSTYPE),Windows_NT)

#
# Windows
#
# Note: network_test runs its loopback servers on threads (services thread
# api), so it builds and runs on Windows against the full Winsock/OpenSSL
# network implementation (TCP, TLS, messages, DTLS and certificates).
all: dumpmidi dif css2theme play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testc terminal_testg \
     window_testc \
     widget_testc \
     graphics_test random_test testviewer window_test widget_test widgets_demo \
     window_race_test \
     sound_test sound_testg network_test services_test stdio_test event eventg term termg hello hellog wshot dshot snake snakeg mine mineg \
     wator watorg pong pongg breakout breakoutw breakoutg breakoutwg backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 line2 \
     line4 line5 clock calc txttest pdftest \
     graph_server breakoutgr breakoutwgr window_race_testr \
     graphics_testr window_testr widget_testr

else ifeq ($(OSTYPE),Darwin)

#
# Mac OS X
#
all: dumpmidi dif css2theme play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg playtextmidi playtextmidig printdev printdevg connectmidi \
     connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testc terminal_testg \
     window_testc \
     widget_testc \
     graphics_test random_test testviewer window_test widget_test widgets_demo \
     window_race_test \
     sound_test sound_testg network_test services_test stdio_test event eventg term termg hello hellog wshot dshot snake snakeg mine mineg \
     wator watorg pong pongg breakout breakoutw breakoutg breakoutwg backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock calc

else ifeq ($(OSTYPE),FreeBSD)

#
# BSD
#
all: dumpmidi dif css2theme play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testc terminal_testg \
     window_testc \
     widget_testc \
     graphics_test random_test testviewer window_test widget_test widgets_demo \
     window_race_test \
     sound_test sound_testg network_test services_test stdio_test event eventg term termg hello hellog wshot dshot snake snakeg mine mineg \
     wator watorg pong pongg breakout breakoutw breakoutg breakoutwg backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock calc
    
else

#
# Linux
#
all: dumpmidi dif css2theme play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testc terminal_testg \
     window_testc \
     widget_testc \
     graphics_test random_test testviewer window_test widget_test widgets_demo \
     window_race_test \
     sound_test sound_testg network_test services_test stdio_test event eventg term termg hello hellog wshot dshot snake snakeg mine mineg \
     wator watorg pong pongg breakout breakoutw breakoutg breakoutwg backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock calc txttest pdftest \
     graph_server graphics_testr window_testr widget_testr
    
endif 

################################################################################
#
# Build individual components
#
################################################################################

#
# Linux library components
#
linux/stdio.o: libc/stdio.c libc/stdio.h Makefile
	$(CC) $(CFLAGS) $(STDIOVIS) -c libc/stdio.c -o linux/stdio.o

linux/services.o: linux/services.c include/services.h Makefile
	$(CC) $(CFLAGS) -c linux/services.c -o linux/services.o
	
linux/sound.o: linux/sound.c include/sound.h Makefile
	$(CC) $(CFLAGS) -c linux/sound.c -lasound -lm -pthread -o linux/sound.o
	
linux/network.o: linux/network.c include/network.h Makefile
	$(CC) $(CFLAGS) -c linux/network.c -o linux/network.o
	
linux/fluidsynthplug.o: linux/fluidsynthplug.c include/sound.h Makefile
	$(CC) $(CFLAGS) -c linux/fluidsynthplug.c -lasound -lm -pthread -o linux/fluidsynthplug.o
	
linux/dumpsynthplug.o: linux/dumpsynthplug.c include/sound.h Makefile
	$(CC) $(CFLAGS) -c linux/dumpsynthplug.c -lasound -lm -pthread -o linux/dumpsynthplug.o
	
linux/terminal.o: linux/terminal.c include/terminal.h Makefile
	$(CC) $(CFLAGS) -c linux/terminal.c -o linux/terminal.o
	
linux/x11/graphics.o: linux/x11/graphics.c include/graphics.h Makefile
	$(CC) $(CFLAGS) -Ilinux -c linux/x11/graphics.c -o linux/x11/graphics.o

#
# The Wayland graphics backend: same library contract as the X backend,
# selected by linking these objects in its place. pdisplay carries the
# platform display interface (pdisplay.h) on Wayland: the window tree,
# rasterization, input, and presentation the backend draws through.
#
linux/wayland/graphics.o: linux/wayland/graphics.c \
	linux/wayland/graphics_i.h linux/wayland/pdisplay.h include/graphics.h \
	Makefile
	$(CC) $(CFLAGS) -Ilinux -c linux/wayland/graphics.c \
	    -o linux/wayland/graphics.o

# The desktop decorations: window frames and onscreen menus, one module per
# desktop, holding just what that desktop draws differently. Both link into
# every program and the one whose desktop is running registers itself at
# load, the same way the widget packages do. graphics_i.h is the seam.
linux/wayland/gnome/decorations.o: linux/wayland/gnome/decorations.c \
	linux/wayland/graphics_i.h linux/wayland/pdisplay.h include/graphics.h \
	Makefile
	$(CC) $(CFLAGS) -Ilinux -c linux/wayland/gnome/decorations.c \
	    -o linux/wayland/gnome/decorations.o

linux/wayland/plasma/decorations.o: linux/wayland/plasma/decorations.c \
	linux/wayland/graphics_i.h linux/wayland/pdisplay.h include/graphics.h \
	Makefile
	$(CC) $(CFLAGS) -Ilinux -c linux/wayland/plasma/decorations.c \
	    -o linux/wayland/plasma/decorations.o

# pdisplay carries the rasterizer's pixel loops; the optimizer is worth
# 2x-24x across the primitive benchmarks there, and -O3 buys a further
# 1.3x-2x on fills and lines over -O2 (arcs give a little back). The
# header benchmark table in tests/graphics_test.c was recorded at -O3.
# -g3 stays for symbols; set PDISPLAY_OPT= (empty) if stepping through
# this file matters more than speed, or add -march=native locally for
# the last measure at the cost of binary portability
PDISPLAY_OPT ?= -O3
linux/wayland/pdisplay.o: linux/wayland/pdisplay.c linux/wayland/pdisplay.h \
	linux/wayland/wlproto/xdg-shell-client-protocol.h Makefile
	$(CC) $(CFLAGS) $(PDISPLAY_OPT) -Ilinux -c linux/wayland/pdisplay.c \
	    -o linux/wayland/pdisplay.o

linux/wayland/wlproto/xdg-shell-protocol.o: \
	linux/wayland/wlproto/xdg-shell-protocol.c Makefile
	$(CC) $(CFLAGS) -c linux/wayland/wlproto/xdg-shell-protocol.c \
	    -o linux/wayland/wlproto/xdg-shell-protocol.o

# Screen capture for the test programs: not part of the library, since it
# is only linked by the tests that call it and carries libpng with it.
linux/wayland/screen_capture.o: linux/wayland/screen_capture.c \
	linux/wayland/graphics_i.h linux/wayland/pdisplay.h Makefile
	$(CC) $(CFLAGS) -Ilinux -c linux/wayland/screen_capture.c \
	    -o linux/wayland/screen_capture.o

WLGRAPH = linux/wayland/graphics.o linux/wayland/gnome/decorations.o \
	linux/wayland/plasma/decorations.o linux/wayland/pdisplay.o \
	linux/wayland/wlproto/xdg-shell-protocol.o
WLLIBS = -lwayland-client -lwayland-cursor -lxkbcommon

linux/system_event.o: linux/system_event.c linux/system_event.h Makefile
	$(CC) $(CFLAGS) -c linux/system_event.c -o linux/system_event.o
	

linux/screen_capture.o: linux/screen_capture.c Makefile
	$(CC) $(CFLAGS) -c linux/screen_capture.c -o linux/screen_capture.o

# automatic event input for the picture tests (see doc/auto_events.md)
tests/auto_event.o: tests/auto_event.c include/graphics.h Makefile
	$(CC) $(CFLAGS) -c tests/auto_event.c -o tests/auto_event.o

# the same for the terminal tests, with the terminal's event set
tests/auto_eventt.o: tests/auto_event.c include/terminal.h Makefile
	$(CC) $(CFLAGS) -DAUTO_EVENT_TERMINAL -c tests/auto_event.c \
	    -o tests/auto_eventt.o

linux/terminal_capture.o: linux/terminal_capture.c Makefile
	$(CC) $(CFLAGS) -c linux/terminal_capture.c -o linux/terminal_capture.o
	
#
# Windows library components
#
# Note that stub sources are not yet implemented
#
windows/stdio.o: libc/stdio.c libc/stdio.h Makefile
	$(CC) $(CFLAGS) -c libc/stdio.c -o windows/stdio.o
	
windows/services.o: windows/services.c include/services.h Makefile
	$(CC) $(CFLAGS) -c windows/services.c -o windows/services.o
	
windows/sound.o: windows/sound.c include/sound.h Makefile
	$(CC) $(CFLAGS) -c windows/sound.c -o windows/sound.o
	
windows/network.o: windows/network.c include/network.h Makefile
	$(CC) $(CFLAGS) -c windows/network.c -o windows/network.o
	
windows/terminal.o: windows/terminal.c include/terminal.h Makefile
	$(CC) $(CFLAGS) -c windows/terminal.c -o windows/terminal.o
	
windows/graphics.o: windows/graphics.c include/graphics.h Makefile
	$(CC) $(CFLAGS) -c windows/graphics.c -o windows/graphics.o

windows/screen_capture.o: windows/screen_capture.c Makefile
	$(CC) $(CFLAGS) -c windows/screen_capture.c -o windows/screen_capture.o

#
# Mac OS X library components
#
# Note that stub sources are not yet implemented.
#
# Mac OS X can use some of the same components as Linux.
#
macosx/stdio.o: libc/stdio.c libc/stdio.h Makefile
	$(CC) $(CFLAGS) -c libc/stdio.c -o macosx/stdio.o
	
macosx/services.o: linux/services.c include/services.h Makefile
	$(CC) $(CFLAGS) -c linux/services.c -o macosx/services.o
	
macosx/sound.o: macosx/sound.c include/sound.h Makefile
	$(CC) $(CFLAGS) -c macosx/sound.c -o macosx/sound.o
	
macosx/network.o: linux/network.c include/network.h Makefile
	$(CC) $(CFLAGS) -c linux/network.c -o macosx/network.o
	
macosx/terminal.o: linux/terminal.c include/terminal.h Makefile
	$(CC) $(CFLAGS) -c linux/terminal.c -o macosx/terminal.o
	
macosx/graphics_cocoa.o: macosx/graphics_cocoa.m macosx/pa_cocoa.h Makefile
	$(CC) $(CFLAGS) -fobjc-arc -c macosx/graphics_cocoa.m \
	-o macosx/graphics_cocoa.o

macosx/graphics.o: macosx/graphics.c macosx/pa_cocoa.h include/graphics.h Makefile
	$(CC) $(CFLAGS) -c macosx/graphics.c -o macosx/graphics.o
	
macosx/system_event.o: macosx/system_event.c linux/system_event.h Makefile
	$(CC) $(CFLAGS) -fPIC -c macosx/system_event.c -o macosx/system_event.o

macosx/screen_capture.o: macosx/screen_capture.c macosx/pa_cocoa.h Makefile
	$(CC) $(CFLAGS) -c macosx/screen_capture.c -o macosx/screen_capture.o
	
#
# BSD library components
#
# Note that stub sources are not yet implemented.
#
# BSD can use some of the same components as Linux.
#
bsd/stdio.o: libc/stdio.c libc/stdio.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c libc/stdio.c -o bsd/stdio.o

bsd/services.o: linux/services.c include/services.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c linux/services.c -o bsd/services.o

bsd/sound.o: linux/sound.c include/sound.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I/usr/local/include -c linux/sound.c -o bsd/sound.o

bsd/fluidsynthplug.o: linux/fluidsynthplug.c include/sound.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I/usr/local/include -c linux/fluidsynthplug.c -o bsd/fluidsynthplug.o

bsd/dumpsynthplug.o: linux/dumpsynthplug.c include/sound.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I/usr/local/include -c linux/dumpsynthplug.c -o bsd/dumpsynthplug.o

bsd/network.o: stub/network.c include/network.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I/usr/local/include \
		-c stub/network.c -o bsd/network.o

bsd/terminal.o: linux/terminal.c include/terminal.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c linux/terminal.c -o bsd/terminal.o

bsd/graphics.o: linux/x11/graphics.c include/graphics.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I/usr/local/include -c linux/x11/graphics.c \
	-o bsd/graphics.o


bsd/system_event.o: macosx/system_event.c linux/system_event.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c macosx/system_event.c -o bsd/system_event.o

bsd/screen_capture.o: stub/screen_capture.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c stub/screen_capture.c -o bsd/screen_capture.o

#
# Components in common to all systems
#
utils/config.o: utils/config.c include/localdefs.h include/services.h \
	            include/config.h Makefile
	$(CC) $(CFLAGS) -c utils/config.c -o utils/config.o
	
utils/option.o: utils/option.c include/localdefs.h include/services.h \
	            include/option.h Makefile
	$(CC) $(CFLAGS) -c utils/option.c -o utils/option.o
	
stub/keeper.o: stub/keeper.c
	$(CC) $(CFLAGS) -c stub/keeper.c -o stub/keeper.o

cpp/terminal.o: cpp/terminal.cpp
	$(CPP) $(CFLAGS) -Ihpp -c cpp/terminal.cpp -o cpp/terminal.o

cpp/sound.o: cpp/sound.cpp
	$(CPP) $(CFLAGS) -Ihpp -c cpp/sound.cpp -o cpp/sound.o

cpp/services.o: cpp/services.cpp
	$(CPP) $(CFLAGS) -Ihpp -c cpp/services.cpp -o cpp/services.o

cpp/network.o: cpp/network.cpp
	$(CPP) $(CFLAGS) -Ihpp -c cpp/network.cpp -o cpp/network.o

cpp/graphics.o: cpp/graphics.cpp
	$(CPP) $(CFLAGS) -Ihpp -c cpp/graphics.cpp -o cpp/graphics.o
	
portable/gnome_widgets.o: portable/gnome_widgets.c
	$(CC) $(CFLAGS) -c portable/gnome_widgets.c \
		-o portable/gnome_widgets.o

portable/plasma_widgets.o: portable/plasma_widgets.c
	$(CC) $(CFLAGS) -c portable/plasma_widgets.c \
		-o portable/plasma_widgets.o

portable/pdfgraph.o: portable/pdfgraph.c include/graphics.h
	$(CC) $(CFLAGS) -c portable/pdfgraph.c \
		-o portable/pdfgraph.o

portable/txtterminal.o: portable/txtterminal.c include/terminal.h
	$(CC) $(CFLAGS) -c portable/txtterminal.c \
		-o portable/txtterminal.o

portable/graph_client.o: portable/graph_client.c include/graphics.h \
	include/network.h include/graph_remote.h
	$(CC) $(CFLAGS) -c portable/graph_client.c \
		-o portable/graph_client.o

stub/screen_capture_stub.o: stub/screen_capture_stub.c
	$(CC) $(CFLAGS) -c stub/screen_capture_stub.c \
		-o stub/screen_capture_stub.o

portable/widget_base.o: portable/widget_base.c
	$(CC) $(CFLAGS) -c portable/widget_base.c \
		-o portable/widget_base.o
		
portable/windowc.o: portable/windowc.c
	$(CC) $(CFLAGS) -c portable/windowc.c \
		-o portable/windowc.o
	
################################################################################
#
# Build libraries
#
################################################################################

#
# Create terminal mode and graphical mode libraries
#

ifeq ($(OSTYPE),Windows_NT)

#
# Windows
#
# Windows cannot use .so files, but rather uses statically linked files that
# reference .dlls at runtime.
#
lib/libami_plain.a: windows/services.o windows/sound.o windows/network.o \
	utils/option.o utils/config.o windows/stdio.o
	ar rcs lib/libami_plain.a windows/services.o windows/sound.o \
        windows/network.o utils/config.o utils/option.o windows/stdio.o
	
lib/libami_term.a: windows/services.o windows/sound.o windows/network.o \
    windows/terminal.o portable/txtterminal.o utils/config.o utils/option.o windows/stdio.o
	ar rcs lib/libami_term.a windows/services.o windows/sound.o \
	    windows/network.o windows/terminal.o portable/txtterminal.o utils/config.o utils/option.o \
	    windows/stdio.o
	
# The termc variant is the terminal library with the character mode window
# manager (windowc) always included. windowc is constructor-registered and
# transparent by default, so nothing references its symbols. Windows links the
# terminal library with --whole-archive (see CLIBS), so every archive member is
# pulled in regardless and windowc can simply be an additional member here.
# Mac OS X has to partial-link it into terminal.o instead, having no
# whole-archive equivalent.
#
lib/libami_termc.a: windows/services.o windows/sound.o windows/network.o \
    windows/terminal.o portable/txtterminal.o portable/windowc.o utils/config.o utils/option.o \
    windows/stdio.o
	ar rcs lib/libami_termc.a windows/services.o windows/sound.o \
	    windows/network.o windows/terminal.o portable/txtterminal.o portable/windowc.o \
	    utils/config.o utils/option.o windows/stdio.o
	
lib/libami_graph.a: windows/services.o windows/sound.o windows/network.o \
    windows/graphics.o portable/pdfgraph.o utils/config.o utils/option.o \
    windows/stdio.o
	ar rcs lib/libami_graph.a windows/services.o windows/sound.o \
	    windows/network.o windows/graphics.o portable/pdfgraph.o \
	    utils/config.o utils/option.o windows/stdio.o
	
else ifeq ($(OSTYPE),Darwin)

#
# Mac OS X
#
# Mac OS X cannot use .so files, but rather uses statically linked files.
#
lib/libami_plain.a: macosx/services.o macosx/sound.o macosx/network.o \
	utils/config.o utils/option.o macosx/stdio.o
	ar rcs lib/libami_plain.a macosx/services.o macosx/sound.o \
        macosx/network.o utils/config.o utils/option.o macosx/stdio.o
	
lib/libami_term.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/system_event.o macosx/terminal.o utils/config.o utils/option.o \
    macosx/stdio.o portable/txtterminal.o
	ar rcs lib/libami_term.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/system_event.o macosx/terminal.o \
	    utils/config.o utils/option.o macosx/stdio.o portable/txtterminal.o

# The termc variant is the terminal library with the character mode window
# manager (windowc) always included. windowc is constructor-registered and
# transparent by default, so nothing references its symbols; partial-link it
# with terminal.o into one member (ld -r) so archive selectivity cannot drop
# it when a program references only the terminal API.
macosx/termc.o: macosx/terminal.o portable/windowc.o
	ld -r -o macosx/termc.o macosx/terminal.o portable/windowc.o

lib/libami_termc.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/system_event.o macosx/termc.o utils/config.o utils/option.o \
    macosx/stdio.o portable/txtterminal.o
	ar rcs lib/libami_termc.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/system_event.o macosx/termc.o \
	    utils/config.o utils/option.o macosx/stdio.o portable/txtterminal.o

lib/libami_graph.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/system_event.o macosx/graphics.o macosx/graphics_cocoa.o \
    utils/config.o utils/option.o macosx/stdio.o portable/pdfgraph.o portable/widget_base.o
	ar rcs lib/libami_graph.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/system_event.o macosx/graphics.o \
	    macosx/graphics_cocoa.o utils/config.o utils/option.o macosx/stdio.o portable/pdfgraph.o portable/widget_base.o

else ifeq ($(OSTYPE),FreeBSD)

#
# BSD
#
# Use statically linked files, for BSD
#
lib/libami_plain.a: bsd/services.o bsd/sound.o bsd/fluidsynthplug.o \
	bsd/dumpsynthplug.o bsd/network.o utils/config.o utils/option.o bsd/stdio.o
	ar rcs lib/libami_plain.a bsd/services.o bsd/sound.o \
	    bsd/fluidsynthplug.o bsd/dumpsynthplug.o \
        bsd/network.o utils/config.o utils/option.o bsd/stdio.o

lib/libami_term.a: bsd/services.o bsd/sound.o bsd/fluidsynthplug.o \
	bsd/dumpsynthplug.o bsd/network.o \
    bsd/system_event.o bsd/terminal.o utils/config.o utils/option.o \
    bsd/stdio.o
	ar rcs lib/libami_term.a bsd/services.o bsd/sound.o \
	    bsd/fluidsynthplug.o bsd/dumpsynthplug.o \
	    bsd/network.o bsd/system_event.o bsd/terminal.o \
	    utils/config.o utils/option.o bsd/stdio.o

lib/libami_graph.a: bsd/services.o bsd/sound.o bsd/fluidsynthplug.o \
	bsd/dumpsynthplug.o bsd/network.o \
    bsd/graphics.o bsd/system_event.o \
	portable/gnome_widgets.o portable/plasma_widgets.o portable/widget_base.o utils/config.o utils/option.o bsd/stdio.o
	ar rcs lib/libami_graph.a bsd/services.o bsd/sound.o \
	    bsd/fluidsynthplug.o bsd/dumpsynthplug.o \
	    bsd/network.o bsd/system_event.o bsd/graphics.o \
	    portable/gnome_widgets.o portable/plasma_widgets.o portable/widget_base.o utils/config.o utils/option.o bsd/stdio.o
	
else

#
# Linux
#

#
# Note that sound lib cannot be put into an .so, there is a bug in ALSA.
# Thus we leave it as a .o file.
#
# The linux build uses fluidsynth, and uses a series of runtime plug-ins
# to do things like midi to wave conversion.
#
lib/petit_ami_plain.so: $(LINUXSTDIO) linux/services.o linux/network.o utils/config.o \
    utils/option.o
	$(CC) -shared $(LINUXSTDIO) linux/services.o linux/network.o utils/config.o \
		utils/option.o -o lib/petit_ami_plain.so
	
lib/petit_ami_term.so: $(LINUXSTDIO) linux/services.o linux/network.o \
	linux/terminal.o $(WINDOWC) linux/system_event.o utils/config.o utils/option.o \
    cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o
	$(CC) -shared $(LINUXSTDIO) linux/services.o linux/network.o \
		linux/terminal.o $(WINDOWC) linux/system_event.o utils/config.o \
		utils/option.o  cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o -lstdc++ -o lib/petit_ami_term.so
	
#
# Terminal library with the character mode window manager always included.
# This is a separate file from lib/petit_ami_term.so so that both the plain
# and the managed configurations can exist at once: they were previously the
# same file, and building one silently replaced the other.
#
lib/petit_ami_termc.so: $(LINUXSTDIO) linux/services.o linux/network.o \
	linux/terminal.o portable/windowc.o linux/system_event.o utils/config.o \
	utils/option.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o
	$(CC) -shared $(LINUXSTDIO) linux/services.o linux/network.o \
		linux/terminal.o portable/windowc.o linux/system_event.o \
		utils/config.o utils/option.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o -lstdc++ \
		-o lib/petit_ami_termc.so

lib/petit_ami_graph.so: $(LINUXSTDIO) linux/services.o linux/network.o \
	linux/x11/graphics.o linux/system_event.o \
	portable/gnome_widgets.o portable/plasma_widgets.o portable/widget_base.o utils/config.o utils/option.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o \
	cpp/graphics.o
	$(CC) -shared $(LINUXSTDIO) linux/services.o linux/network.o \
		linux/x11/graphics.o linux/system_event.o \
		portable/gnome_widgets.o portable/plasma_widgets.o portable/widget_base.o utils/config.o utils/option.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o \
		cpp/graphics.o -lstdc++ -o lib/petit_ami_graph.so

#
# The Linux static configuration: per model, one bundle archive holding
# Petit-Ami's code. The system libraries -- glibc, ALSA, fluidsynth,
# OpenSSL, X -- stay shared: ALSA and PulseAudio do not support static
# operation, so neither does Petit-Ami, and sound mixes with the desktop
# through ALSA's shared PulseAudio bridge as usual.
#
# Each archive holds three members:
#
#   <model>_core.o  the model's presentation, services, stdio glue and
#                   configuration, partial-linked (ld -r) into one member
#                   so the constructor-registered modules (windowc, the
#                   widget packages) cannot be dropped by archive
#                   selectivity. A program that makes no calls of its
#                   own is promoted by keeper.o referencing into it.
#   sound.o         the sound module and both synthesizer plugins,
#                   partial-linked so the plugins' constructor
#                   registrations ride with the member. Pulled only when
#                   the program uses sound, and with it the shared
#                   libasound/libfluidsynth dependencies; programs
#                   without sound carry neither.
#   network.o       the network module, pulled only when used, and with
#                   it the shared OpenSSL dependency.
#
# So programs get what they use, as always. A program link line is:
#
#     gcc prog.c stub/keeper.o -Llib -lami_term \
#         -lasound -lfluidsynth -lssl -lcrypto -lstdc++ -lm -lpthread
#
# with the trailing libraries dropped by as-needed linking when the
# member that wants them is not pulled.
#

# the sound bundle: module and plugins whole, one member
lib/sound.o: linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o
	ld -r -o lib/sound.o \
	    linux/sound.o linux/fluidsynthplug.o linux/dumpsynthplug.o

# the model cores
CORE_COMMON = $(LINUXSTDIO) linux/services.o utils/config.o utils/option.o

lib/plain_core.o: $(CORE_COMMON) cpp/sound.o cpp/services.o cpp/network.o
	ld -r -o lib/plain_core.o $(CORE_COMMON) cpp/sound.o cpp/services.o cpp/network.o

lib/term_core.o: $(CORE_COMMON) linux/terminal.o $(WINDOWC) \
	portable/txtterminal.o \
	linux/system_event.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o
	ld -r -o lib/term_core.o $(CORE_COMMON) linux/terminal.o $(WINDOWC) \
	    portable/txtterminal.o \
	    linux/system_event.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o

lib/termc_core.o: $(CORE_COMMON) linux/terminal.o portable/windowc.o \
	portable/txtterminal.o \
	linux/system_event.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o
	ld -r -o lib/termc_core.o $(CORE_COMMON) linux/terminal.o \
	    portable/windowc.o portable/txtterminal.o \
	    linux/system_event.o cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o

lib/graph_core.o: $(CORE_COMMON) linux/x11/graphics.o linux/system_event.o \
	portable/widget_base.o portable/gnome_widgets.o portable/plasma_widgets.o portable/pdfgraph.o \
	cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o \
	cpp/graphics.o
	ld -r -o lib/graph_core.o $(CORE_COMMON) linux/x11/graphics.o \
	    linux/system_event.o portable/widget_base.o portable/gnome_widgets.o portable/plasma_widgets.o \
	    portable/pdfgraph.o \
	    cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o cpp/graphics.o

# the model archives
lib/libami_plain.a: lib/plain_core.o lib/sound.o linux/network.o
	rm -f lib/libami_plain.a
	ar rcs lib/libami_plain.a lib/plain_core.o lib/sound.o linux/network.o

lib/libami_term.a: lib/term_core.o lib/sound.o linux/network.o
	rm -f lib/libami_term.a
	ar rcs lib/libami_term.a lib/term_core.o lib/sound.o linux/network.o

lib/libami_termc.a: lib/termc_core.o lib/sound.o linux/network.o
	rm -f lib/libami_termc.a
	ar rcs lib/libami_termc.a lib/termc_core.o lib/sound.o linux/network.o

lib/libami_graph.a: lib/graph_core.o lib/sound.o linux/network.o
	rm -f lib/libami_graph.a
	ar rcs lib/libami_graph.a lib/graph_core.o lib/sound.o linux/network.o

# the Wayland graphics model: the wayland backend objects in place of
# linux/x11/graphics.o, all else identical
lib/graphw_core.o: $(CORE_COMMON) $(WLGRAPH) linux/system_event.o \
	portable/widget_base.o portable/gnome_widgets.o portable/plasma_widgets.o portable/pdfgraph.o \
	cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o \
	cpp/graphics.o
	ld -r -o lib/graphw_core.o $(CORE_COMMON) $(WLGRAPH) \
	    linux/system_event.o portable/widget_base.o portable/gnome_widgets.o portable/plasma_widgets.o \
	    portable/pdfgraph.o \
	    cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o cpp/graphics.o

lib/libami_graphw.a: lib/graphw_core.o lib/sound.o linux/network.o
	rm -f lib/libami_graphw.a
	ar rcs lib/libami_graphw.a lib/graphw_core.o lib/sound.o linux/network.o

endif

################################################################################
#
# Build final programs
#
################################################################################

#
# Cross system tools - These work on any Petit-Ami compliant environment
#	

#
# Dump midi file in readable format (not Petit-Ami dependent)
#
dumpmidi: utils/dumpmidi.c Makefile
	$(CC) utils/dumpmidi.c -o bin/dumpmidi

#
# File difference utility with '?' wildcard compare, used by bin/regress
# (not Petit-Ami dependent)
#
dif: utils/dif.c Makefile
	$(CC) utils/dif.c -o bin/dif

#
# Show what the X clipboard contains: owner, formats, text or image
# content. The answer to a screen capture that silently left stale data.
#
showclip: utils/showclip.c Makefile
	$(CC) utils/showclip.c -lX11 -o bin/showclip

#
# Convert a GTK CSS theme to a Petit-Ami widget theme in petit_ami.cfg
# (not Petit-Ami dependent)
#
css2theme: utils/css2theme.c Makefile
	$(CC) utils/css2theme.c -o bin/css2theme

#
# GTK version of the widget sampler in test.c, as the reference to compare
# the widget theme against. Not built by "make all": it links GTK, which
# Petit-Ami deliberately does not.
#
test_gtk: test_gtk.c Makefile
	$(CC) test_gtk.c `pkg-config --cflags --libs gtk+-3.0` -o test_gtk

#
# Managerc subwindow test. The terminal library must be built with windowc
# in it: make lib/petit_ami_term.so USEWINDOWC=1
#
#
# Managerc tests. These link the manager carrying library, so they need no
# build flag: the plain and managed libraries coexist.
#
test_manager: test_manager.c lib/petit_ami_termc.so Makefile
	$(CC) $(CFLAGS) test_manager.c $(CLIBSC) -o test_manager

test_manager2: test_manager2.c lib/petit_ami_termc.so Makefile
	$(CC) $(CFLAGS) test_manager2.c $(CLIBSC) -o test_manager2

#
# General test program
#
# If the build is not appropriate to the test program, you can either ignore
# the errors or comment this out. The test target will move out of the makefile
# in time.
#
test: $(PLIBSD) test.c
	$(CC) $(CFLAGS) test.c $(PLIBS) -o test
	
testc: $(CLIBSD) test.c
	$(CC) $(CFLAGS) test.c $(CLIBS) -o testc
	
testg: $(GLIBSD) test.c
	$(CC) $(CFLAGS) test.c -Wl,-u,ami_cursorg $(GLIBS) -o testg

#
# Widget demonstrator: a graphics window with the demonstration widget
# from portable/widget_demo.c centered in it, for examination and test
#
test_demo: $(GLIBSD) test_demo.c portable/widget_demo.c
	$(CC) $(CFLAGS) test_demo.c portable/widget_demo.c $(GLIBS) -o test_demo
	
test+: $(PLIBSD) test.cp
	$(CPP) $(CFLAGS) test.cp $(PLIBS) -o test
	
test++: $(PLIBSD) test.cpp
	$(CPP) $(CFLAGS) test.cpp $(PLIBS) -o test
	
testc+: $(CLIBSCPPD) test.cp
	$(CPP) $(CFLAGSCPP) test.cp $(CLIBSCPP) -o testc
	
testc++: $(CLIBSCPPD) test.cpp
	$(CPP) $(CFLAGSCPP) test.cpp $(CLIBSCPP) -o testc
	
testg++: $(GLIBSD) test.cpp
	$(CPP) $(CFLAGS) test.cpp -Wl,-u,ami_cursorg $(GLIBS) -o testg
	
#
# Target programs that use Petit-Ami, such as games, utilities, etc.
# "dazzler" programs is my term. It comes from the Cromemco Dazzler, a graphics
# that ran on early S100 computers, and was a popular display demonstration from
# those days.
#
# Note serial output model programs can be ported to each of the console or
# graphical models, but generally the console model is skipped since the action
# such a program would be identical to the serial model.
#
# The use case for graphical versions of serial model programs is not clear.
# They serve as a test of the promotion model, and might be useful if only a
# graphical model system is in use, like the file manager.
#

#
# Play example songs in QBasic play format (uses console timers)
# 	
play: $(CLIBSD) sound_programs/play.c
	$(CC) $(CFLAGS) sound_programs/play.c $(CLIBS) -o bin/play
	
playg: $(GLIBSD) sound_programs/play.c
	$(CC) $(CFLAGS) sound_programs/play.c $(GLIBS) -o bin/playg

#
# Emulate a sound keyboard (uses console timers)
#	
keyboard: $(CLIBSD) sound_programs/keyboard.c
	$(CC) $(CFLAGS) sound_programs/keyboard.c $(CLIBS) -o bin/keyboard
	
keyboardg: $(GLIBSD) sound_programs/keyboard.c
	$(CC) $(CFLAGS) sound_programs/keyboard.c $(GLIBS) -o bin/keyboardg

#
# Play midi files
#	
playmidi: $(PLIBSD) sound_programs/playmidi.c
	$(CC) $(CFLAGS) sound_programs/playmidi.c $(PLIBS) -o bin/playmidi
	
playmidig: $(GLIBSD) sound_programs/playmidi.c
	$(CC) $(CFLAGS) sound_programs/playmidi.c $(GLIBS) -o bin/playmidig

#
# Play text midi files (mf2t/t2mf format)
#
playtextmidi: $(PLIBSD) sound_programs/playtextmidi.c
	$(CC) $(CFLAGS) sound_programs/playtextmidi.c $(PLIBS) -o bin/playtextmidi

playtextmidig: $(GLIBSD) sound_programs/playtextmidi.c
	$(CC) $(CFLAGS) sound_programs/playtextmidi.c $(GLIBS) -o bin/playtextmidig

#
# Play wave files
#
playwave: $(PLIBSD) sound_programs/playwave.c
	$(CC) $(CFLAGS) sound_programs/playwave.c $(PLIBS) -o bin/playwave
	
playwaveg: $(GLIBSD) sound_programs/playwave.c
	$(CC) $(CFLAGS) sound_programs/playwave.c $(GLIBS) -o bin/playwaveg

#
# Print a list of available sound devices
#	
printdev: $(PLIBSD) sound_programs/printdev.c
	$(CC) $(CFLAGS) sound_programs/printdev.c $(PLIBS) -o bin/printdev
	
printdevg: $(GLIBSD) sound_programs/printdev.c
	$(CC) $(CFLAGS) sound_programs/printdev.c $(GLIBS) -o bin/printdevg

#
# Connect Midi input port to Midi output port
#
connectmidi: $(PLIBSD) sound_programs/connectmidi.c
	$(CC) $(CFLAGS) sound_programs/connectmidi.c $(PLIBS) -o bin/connectmidi
	
connectmidig: $(GLIBSD) sound_programs/connectmidi.c
	$(CC) $(CFLAGS) sound_programs/connectmidi.c $(GLIBS) -o bin/connectmidig

#
# Connect wave input port to wave output port
#	
connectwave: $(PLIBSD) sound_programs/connectwave.c
	$(CC) $(CFLAGS) sound_programs/connectwave.c $(PLIBS) -o bin/connectwave
	
connectwaveg: $(GLIBSD) sound_programs/connectwave.c
	$(CC) $(CFLAGS) sound_programs/connectwave.c $(GLIBS) -o bin/connectwaveg

#	
# Play random notes
#
random: $(CLIBSD) sound_programs/random.c
	$(CC) $(CFLAGS) sound_programs/random.c $(CLIBS) -o bin/random
	
randomg: $(GLIBSD) sound_programs/random.c
	$(CC) $(CFLAGS) sound_programs/random.c $(GLIBS) -o bin/randomg

#
# Generate waveforms
#	
genwave: $(PLIBSD) sound_programs/genwave.c
	$(CC) $(CFLAGS) sound_programs/genwave.c $(PLIBS) -o bin/genwave
	
genwaveg: $(GLIBSD) sound_programs/genwave.c
	$(CC) $(CFLAGS) sound_programs/genwave.c $(GLIBS) -o bin/genwaveg

#
# Screen capture object (platform-dependent)
#
# SCREEN_CAPTURE_OBJ serves the terminal model programs, GSCREEN_CAPTURE_OBJ
# the graphical ones. On Linux they part company: a graphical program's
# capture is a picture, read from its own window; a terminal program's
# screen belongs to the terminal emulator, and its capture is characters,
# the page the terminal prints when asked (see linux/terminal_capture.c).
#
ifeq ($(OSTYPE),Windows_NT)
SCREEN_CAPTURE_OBJ = windows/screen_capture.o
GSCREEN_CAPTURE_OBJ = $(SCREEN_CAPTURE_OBJ)
else ifeq ($(OSTYPE),Darwin)
SCREEN_CAPTURE_OBJ = macosx/screen_capture.o
GSCREEN_CAPTURE_OBJ = $(SCREEN_CAPTURE_OBJ)
else ifeq ($(OSTYPE),FreeBSD)
SCREEN_CAPTURE_OBJ = bsd/screen_capture.o
GSCREEN_CAPTURE_OBJ = $(SCREEN_CAPTURE_OBJ)
else ifeq ($(GRAPHICS_BACKEND),wayland)
SCREEN_CAPTURE_OBJ = linux/terminal_capture.o
GSCREEN_CAPTURE_OBJ = linux/wayland/screen_capture.o
else ifeq ($(GRAPHICS_BACKEND),fb)
# the graphical model captures the stack's own composed screen
SCREEN_CAPTURE_OBJ = linux/terminal_capture.o
GSCREEN_CAPTURE_OBJ = linux/wayland/screen_capture.o
else
SCREEN_CAPTURE_OBJ = linux/terminal_capture.o
GSCREEN_CAPTURE_OBJ = linux/screen_capture.o
endif

#
# Link set for programs stacked on windowc over terminal. Same as CLIBS but
# with the manager carrying library in place of the plain one.
#
CLIBSC = $(subst ami_term.,ami_termc.,$(CLIBS))

#
# Test console model compliant output
#
ifeq ($(OSTYPE),Darwin)
terminal_test: $(CLIBSD) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ) $(CLIBS) -o bin/terminal_test
else ifeq ($(OSTYPE),Windows_NT)
# Windows screen capture uses GDI, not X11, so libpng/zlib are required but
# libX11 is not linked.
terminal_test: $(CLIBSD) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ) $(CLIBS) -lpng -lz -o bin/terminal_test
else
terminal_test: $(CLIBSD) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ) $(CLIBS) -o bin/terminal_test
endif

ifeq ($(OSTYPE),Darwin)
terminal_testg: $(GLIBSD) tests/terminal_test.c tests/auto_eventt.o $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(GSCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/terminal_testg
else
terminal_testg: $(GLIBSD) tests/terminal_test.c tests/auto_eventt.o $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(GSCREEN_CAPTURE_OBJ) $(GLIBS) $(XLIBS) -o bin/terminal_testg
endif

#
# Test graph model compliant output
#
ifeq ($(OSTYPE),Darwin)
graphics_test: $(GLIBSD) tests/graphics_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/graphics_test
else
graphics_test: $(GLIBSD) tests/graphics_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ) $(GLIBS) $(XLIBS) -o bin/graphics_test
endif

#
# Random torture test: random procedures of the API on several threads at
# once, until cancelled (see doc/random_test.md). Graphical model, with the
# network and sound modules the graphical libraries carry.
#
ifeq ($(OSTYPE),Darwin)
random_test: $(GLIBSD) tests/random_test.c
	$(CC) $(CFLAGS) tests/random_test.c $(GLIBS) -o bin/random_test
else
random_test: $(GLIBSD) tests/random_test.c
	$(CC) $(CFLAGS) tests/random_test.c $(GLIBS) $(XLIBS) -o bin/random_test
endif

#
# Graphics test on the Wayland backend: the same program, the backend
# swapped by link option
#
graphics_testw: $(GLIBSWD) tests/graphics_test.c tests/auto_event.o \
	linux/wayland/screen_capture.o
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o linux/wayland/screen_capture.o \
	    $(GLIBSW) $(WLLIBS) -lasound -lfluidsynth -lssl -lcrypto -lstdc++ \
	    -lfreetype -lfontconfig -lm -lpthread -lpng -lz -o bin/graphics_testw

# The GTK edition of the graphics_test benchmarks, for rasterizer and
# complexity comparison against the Ami backends. Needs libgtk-4-dev.
graphics_test_gtk: tests/graphics_test_gtk.c
	$(CC) $(CFLAGS) tests/graphics_test_gtk.c \
	    $(shell pkg-config --cflags --libs gtk4) -lm \
	    -o bin/graphics_test_gtk

widget_testw: $(GLIBSWD) tests/widget_test.c
	$(CC) $(CFLAGS) tests/widget_test.c \
	    $(GLIBSW) $(WLLIBS) -lasound -lfluidsynth -lssl -lcrypto -lstdc++ \
	    -lfreetype -lfontconfig -lm -lpthread -o bin/widget_testw

#
# BMP-stream frame viewer: walks the test_images.img file produced by
# screen_capture, one frame per keypress (left/right arrows).
#
ifeq ($(OSTYPE),Windows_NT)
testviewer: windows/testviewer.c Makefile
	$(CC) -g3 windows/testviewer.c -lpng -lz -lgdi32 -o bin/testviewer
else ifeq ($(OSTYPE),Darwin)
testviewer: macosx/testviewer.c Makefile
	$(CC) -g3 -x objective-c macosx/testviewer.c \
	    -framework Cocoa -framework CoreGraphics -framework ImageIO \
	    -o bin/testviewer
else ifeq ($(OSTYPE),FreeBSD)
testviewer: linux/testviewer.c Makefile
	$(CC) -g3 -I/usr/local/include linux/testviewer.c -L/usr/local/lib -lX11 -lpng -o bin/testviewer
else
testviewer: linux/testviewer.c Makefile
	$(CC) -g3 linux/testviewer.c -lX11 -lpng -o bin/testviewer
endif

#
# Test console model compliant output, stacked on the character mode window
# manager: the same test as terminal_test, but running through
# windowc over terminal rather than terminal alone. The test should behave
# identically, since windowc presents the terminal API transparently when
# the program does not open windows of its own.
#
ifeq ($(OSTYPE),Darwin)
terminal_testc: $(LIBPFX)termc$(LIBEXT) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ) $(CLIBSC) -o bin/terminal_testc
else ifeq ($(OSTYPE),Windows_NT)
terminal_testc: $(LIBPFX)termc$(LIBEXT) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ) $(CLIBSC) -lpng -lz -o bin/terminal_testc
else
terminal_testc: $(LIBPFX)termc$(LIBEXT) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c tests/auto_eventt.o $(SCREEN_CAPTURE_OBJ) $(CLIBSC) -o bin/terminal_testc
endif

#
# Test windows management model compliant output
#
# window_test weak-references two optional backend hooks (wg_hold from
# windowg, grx_glassdiff from the framebuffer backend) and guards every call
# with a NULL check. GNU ld resolves an undefined weak reference to NULL, so
# the guards skip a hook the platform lacks. Apple ld64 refuses to leave a
# static-link symbol undefined unless told which ones may be absent: -U marks
# just these two, so they resolve to NULL at load and the guards skip them,
# matching Linux. Do not add stubs for them: a stub would make the guards
# believe the feature exists.
WEAKOPT = -Wl,-U,_wg_hold -Wl,-U,_grx_glassdiff
ifeq ($(OSTYPE),Darwin)
window_test: $(GLIBSD) tests/window_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/window_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ) $(GLIBS) $(WEAKOPT) -o bin/window_test
else
window_test: $(GLIBSD) tests/window_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/window_test.c tests/auto_event.o $(GSCREEN_CAPTURE_OBJ) $(GLIBS) $(XLIBS) -o bin/window_test
endif

#
# Test windows management model compliant output, stacked on the character
# mode window manager. This is window_test with the graphical mode tests
# removed and the character mode ones kept, running through windowc over
# terminal. Every test in the management test has a character form and a
# graphical form; only the character forms can run on a character surface.
#
ifeq ($(OSTYPE),Darwin)
window_testc: $(LIBPFX)termc$(LIBEXT) tests/window_testc.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/window_testc.c $(SCREEN_CAPTURE_OBJ) $(CLIBSC) -o bin/window_testc
else ifeq ($(OSTYPE),Windows_NT)
window_testc: $(LIBPFX)termc$(LIBEXT) tests/window_testc.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/window_testc.c $(SCREEN_CAPTURE_OBJ) $(CLIBSC) -lpng -lz -o bin/window_testc
else
window_testc: $(LIBPFX)termc$(LIBEXT) tests/window_testc.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/window_testc.c $(SCREEN_CAPTURE_OBJ) $(CLIBSC) -o bin/window_testc
endif

#
# The widget busy box, on the desktop toolkits: the same page as
# tests/widgets_demo.c but built on GTK and on Qt, so the three can be set
# beside each other. They link no part of Ami, and so are built without the
# ordinary flags: those carry -Ilibc, which would put Ami's own stdio.h in
# front of the system one. The Qt one is C++ because Qt is: the toolkit has
# no C binding.
#
gtk_widget_demo: tests/gtk_widget_demo.c
	$(CC) -g -O2 tests/gtk_widget_demo.c \
	    `pkg-config --cflags --libs gtk4` -o bin/gtk_widget_demo

kde_widget_demo: tests/kde_widget_demo.cpp
	$(CPP) -g -O2 -fPIC tests/kde_widget_demo.cpp \
	    `pkg-config --cflags --libs Qt6Widgets` -o bin/kde_widget_demo

#
# The widget busy box: every widget on one page, with a button for each
# dialog. Built like the other widget tests.
#
ifeq ($(OSTYPE),Darwin)
widgets_demo: $(GLIBSD) tests/widgets_demo.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/widgets_demo.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/widgets_demo
else
widgets_demo: $(GLIBSD) tests/widgets_demo.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/widgets_demo.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) $(XLIBS) -o bin/widgets_demo
endif

#
# The manual's widget and dialog figures
#
# wshot takes the widget figures, one to a window cut down to its size.
# dshot holds one dialog open while the rig's frame dump pictures it, a
# dialog being unable to picture itself while it waits to be answered.
#
ifeq ($(OSTYPE),Darwin)
wshot: $(GLIBSD) tests/wshot.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/wshot.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/wshot
else
wshot: $(GLIBSD) tests/wshot.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/wshot.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) $(XLIBS) -o bin/wshot
endif

ifeq ($(OSTYPE),Darwin)
dshot: $(GLIBSD) tests/dshot.c
	$(CC) $(CFLAGS) tests/dshot.c $(GLIBS) -o bin/dshot
else
dshot: $(GLIBSD) tests/dshot.c
	$(CC) $(CFLAGS) tests/dshot.c $(GLIBS) $(XLIBS) -o bin/dshot
endif

#
# Test windows widget compliant output
#
ifeq ($(OSTYPE),Darwin)
widget_test: $(GLIBSD) tests/widget_test.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/widget_test.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) \
	    -o bin/widget_test
else ifeq ($(OSTYPE),Windows_NT)
widget_test: $(GLIBSD) tests/widget_test.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/widget_test.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) \
	    -lpng -lz -o bin/widget_test
else
widget_test: $(GLIBSD) tests/widget_test.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/widget_test.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) \
	    $(XLIBS) -o bin/widget_test
endif


#
# Test widget compliant output, character mode
#
# The widget test applied to the character mode window manager over the
# terminal. Every widget test has a character form and a graphical form;
# only the character forms can run on a character surface.
#
widget_testc: $(LIBPFX)termc$(LIBEXT) tests/widget_testc.c
	$(CC) $(CFLAGS) tests/widget_testc.c $(CLIBSC) -o bin/widget_testc
	
#
# Test sound model compliant input/output (uses console timers)
#	
sound_test: $(CLIBSD) tests/sound_test.c
	$(CC) $(CFLAGS) tests/sound_test.c $(CLIBS) -o bin/sound_test 
	
sound_testg: $(GLIBSD) tests/sound_test.c
	$(CC) $(CFLAGS) tests/sound_test.c $(GLIBS) -o bin/sound_testg 
	
#
# Test network library (automated; replaces the manual list in
# network_test.txt). Run from the project root so the TLS test certificates
# are found.
#
network_test: $(PLIBSD) tests/network_test.c
	$(CC) $(CFLAGS) tests/network_test.c $(PLIBS) -o bin/network_test
	
#
# Test services module
#
# Note services test uses a separate program, services_test1, that tests the ability to
# execute a separate program.
#
services_test: $(PLIBSD) tests/services_test.c
	$(CC) $(CFLAGS) tests/services_test.c $(PLIBS) -o bin/services_test
	$(CC) $(CFLAGS) tests/services_test1.c $(PLIBS) -o bin/services_test1

#
# Test the standard I/O library (printf/scanf/file I/O). Self checking.
#
stdio_test: $(PLIBSD) tests/stdio_test.c
	$(CC) $(CFLAGS) tests/stdio_test.c $(PLIBS) -o bin/stdio_test

#
# Test event model (console and graph mode)
#	
event: $(CLIBSD) tests/event.c
	$(CC) $(CFLAGS) tests/event.c $(CLIBS) -o bin/event

txttest: $(CLIBSD) tests/txttest.c
	$(CC) $(CFLAGS) tests/txttest.c $(CLIBS) -o bin/txttest
	
eventg: $(GLIBSD) tests/event.c
	$(CC) $(CFLAGS) tests/event.c $(GLIBS) -o bin/eventg

pdftest: $(GLIBSD) tests/pdftest.c
	$(CC) $(CFLAGS) tests/pdftest.c $(GLIBS) -o bin/pdftest

#
# The graphics test run remotely: linked with graph_client, the display
# side served by graph_server.
#
ifeq ($(OSTYPE),Windows_NT)
# the client carries the sound API itself, so the sound module stays out
graphics_testr: tests/graphics_test.c tests/auto_event.o portable/graph_client.o \
	stub/screen_capture_stub.o windows/services.o windows/network.o \
	utils/config.o utils/option.o windows/stdio.o
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o portable/graph_client.o \
	    stub/screen_capture_stub.o windows/services.o windows/network.o \
	    utils/config.o utils/option.o windows/stdio.o \
	    -lwinmm -lssl -lcrypto -lws2_32 -lcrypt32 -o bin/graphics_testr
else ifeq ($(OSTYPE),Darwin)
# the remote client draws on the server, so no Cocoa backend and no -e
# entry: plain objects, as on Linux, from the macosx tree. Mach-O runs
# constructors in LINK ORDER (priorities are ignored), so the base modules
# come before graph_client.o: its constructor opens sockets, and the network
# constructor clears the per-descriptor table. Base first, layers after.
graphics_testr: tests/graphics_test.c tests/auto_event.o macosx/stdio.o macosx/services.o \
	macosx/network.o utils/config.o utils/option.o \
	portable/graph_client.o stub/screen_capture_stub.o
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o macosx/stdio.o macosx/services.o \
	    macosx/network.o utils/config.o utils/option.o \
	    portable/graph_client.o stub/screen_capture_stub.o \
	    $(SSL_LIBS) -framework CoreFoundation -framework CoreGraphics \
	    -framework ImageIO -framework CoreMIDI -framework AudioToolbox \
	    -framework IOKit -lm -lpthread -o bin/graphics_testr
else
graphics_testr: tests/graphics_test.c tests/auto_event.o portable/graph_client.o \
	stub/screen_capture_stub.o
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o portable/graph_client.o \
	    stub/screen_capture_stub.o $(LINUXSTDIO) linux/services.o \
	    utils/config.o utils/option.o linux/network.o \
	    -lssl -lcrypto -lm -lpthread -o bin/graphics_testr
endif

#
# The management test run remotely: linked with graph_client, the display
# side served by graph_server.
#
ifeq ($(OSTYPE),Windows_NT)
# the client carries the sound API itself, so the sound module stays out
window_testr: tests/window_test.c tests/auto_event.o portable/graph_client.o \
	stub/screen_capture_stub.o windows/services.o windows/network.o \
	utils/config.o utils/option.o windows/stdio.o
	$(CC) $(CFLAGS) tests/window_test.c tests/auto_event.o portable/graph_client.o \
	    stub/screen_capture_stub.o windows/services.o windows/network.o \
	    utils/config.o utils/option.o windows/stdio.o \
	    -lwinmm -lssl -lcrypto -lws2_32 -lcrypt32 -o bin/window_testr
else ifeq ($(OSTYPE),Darwin)
# the remote client draws on the server, so no Cocoa backend and no -e
# entry: plain objects, as on Linux, from the macosx tree. Mach-O runs
# constructors in LINK ORDER (priorities are ignored), so the base modules
# come before graph_client.o: its constructor opens sockets, and the network
# constructor clears the per-descriptor table. Base first, layers after.
window_testr: tests/window_test.c tests/auto_event.o macosx/stdio.o macosx/services.o \
	macosx/network.o utils/config.o utils/option.o \
	portable/graph_client.o stub/screen_capture_stub.o
	$(CC) $(CFLAGS) tests/window_test.c tests/auto_event.o macosx/stdio.o macosx/services.o \
	    macosx/network.o utils/config.o utils/option.o \
	    portable/graph_client.o stub/screen_capture_stub.o \
	    $(SSL_LIBS) -framework CoreFoundation -framework CoreGraphics \
	    -framework ImageIO -framework CoreMIDI -framework AudioToolbox \
	    -framework IOKit -lm -lpthread $(WEAKOPT) -o bin/window_testr
else
window_testr: tests/window_test.c tests/auto_event.o portable/graph_client.o \
	stub/screen_capture_stub.o
	$(CC) $(CFLAGS) tests/window_test.c tests/auto_event.o portable/graph_client.o \
	    stub/screen_capture_stub.o $(LINUXSTDIO) linux/services.o \
	    utils/config.o utils/option.o linux/network.o \
	    -lssl -lcrypto -lm -lpthread -o bin/window_testr
endif

#
# The widget test run remotely: linked with graph_client, the display
# side served by graph_server.
#
ifeq ($(OSTYPE),Windows_NT)
# the client carries the sound API itself, so the sound module stays out
widget_testr: tests/widget_test.c portable/graph_client.o \
	stub/screen_capture_stub.o windows/services.o windows/network.o \
	utils/config.o utils/option.o windows/stdio.o
	$(CC) $(CFLAGS) tests/widget_test.c portable/graph_client.o \
	    stub/screen_capture_stub.o windows/services.o windows/network.o \
	    utils/config.o utils/option.o windows/stdio.o \
	    -lwinmm -lssl -lcrypto -lws2_32 -lcrypt32 -o bin/widget_testr
else ifeq ($(OSTYPE),Darwin)
# the remote client draws on the server, so no Cocoa backend and no -e
# entry: plain objects, as on Linux, from the macosx tree. Mach-O runs
# constructors in LINK ORDER (priorities are ignored), so the base modules
# come before graph_client.o: its constructor opens sockets, and the network
# constructor clears the per-descriptor table. Base first, layers after.
widget_testr: tests/widget_test.c macosx/stdio.o macosx/services.o \
	macosx/network.o utils/config.o utils/option.o \
	portable/graph_client.o stub/screen_capture_stub.o
	$(CC) $(CFLAGS) tests/widget_test.c macosx/stdio.o macosx/services.o \
	    macosx/network.o utils/config.o utils/option.o \
	    portable/graph_client.o stub/screen_capture_stub.o \
	    $(SSL_LIBS) -framework CoreFoundation -framework CoreGraphics \
	    -framework ImageIO -framework CoreMIDI -framework AudioToolbox \
	    -framework IOKit -lm -lpthread -o bin/widget_testr
else
widget_testr: tests/widget_test.c portable/graph_client.o \
	stub/screen_capture_stub.o
	$(CC) $(CFLAGS) tests/widget_test.c portable/graph_client.o \
	    stub/screen_capture_stub.o $(LINUXSTDIO) linux/services.o \
	    utils/config.o utils/option.o linux/network.o \
	    -lssl -lcrypto -lm -lpthread -o bin/widget_testr
endif

#
# Press on the window make/free race: child windows made and freed in a
# tight cycle, titled and drawn on, so the event pump has frames to paint
# for a window this thread is making or freeing.
#
ifeq ($(OSTYPE),Darwin)
window_race_test: $(GLIBSD) tests/window_race_test.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/window_race_test.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/window_race_test
else
window_race_test: $(GLIBSD) tests/window_race_test.c $(GSCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/window_race_test.c $(GSCREEN_CAPTURE_OBJ) $(GLIBS) $(XLIBS) -o bin/window_race_test
endif

#
# The window race test run remotely, which is where the race was found:
# the two threads are then graph_server's own.
#
ifeq ($(OSTYPE),Windows_NT)
# the client carries the sound API itself, so the sound module stays out
window_race_testr: tests/window_race_test.c portable/graph_client.o \
	stub/screen_capture_stub.o windows/services.o windows/network.o \
	utils/config.o utils/option.o windows/stdio.o
	$(CC) $(CFLAGS) tests/window_race_test.c portable/graph_client.o \
	    stub/screen_capture_stub.o windows/services.o windows/network.o \
	    utils/config.o utils/option.o windows/stdio.o \
	    -lwinmm -lssl -lcrypto -lws2_32 -lcrypt32 -o bin/window_race_testr
else
window_race_testr: tests/window_race_test.c portable/graph_client.o \
	stub/screen_capture_stub.o
	$(CC) $(CFLAGS) tests/window_race_test.c portable/graph_client.o \
	    stub/screen_capture_stub.o $(LINUXSTDIO) linux/services.o \
	    utils/config.o utils/option.o linux/network.o \
	    -lssl -lcrypto -lm -lpthread -o bin/window_race_testr
endif

#
# The sound test run remotely: linked with graph_client, the sound
# devices served by graph_server.
#
sound_testr: tests/sound_test.c portable/graph_client.o
	$(CC) $(CFLAGS) tests/sound_test.c portable/graph_client.o \
	    stub/screen_capture_stub.o $(LINUXSTDIO) linux/services.o \
	    utils/config.o utils/option.o linux/network.o \
	    -lssl -lcrypto -lm -lpthread -o bin/sound_testr

#
# The remote display server: the display side of remote mode, standalone.
#
graph_server: $(GLIBSD) portable/graph_server.c include/graph_remote.h
	$(CC) $(CFLAGS) portable/graph_server.c $(GLIBS) -o bin/graph_server

#
# Test terminal characteristics (console and graph mode)
#	
term: $(CLIBSD) tests/term.c
	$(CC) $(CFLAGS) tests/term.c $(CLIBS) -o bin/term
	
termg: $(GLIBSD) tests/term.c
	$(CC) $(CFLAGS) tests/term.c $(GLIBS) -o bin/termg
	
#
# Hello world, in terminal and in graph mode
#
# The program from the plain output example, unchanged, built against the
# terminal library and against the graphical one: the same source runs on a
# terminal and in a window of its own.
#
# A directory of the same name stands beside these, so the two names are
# declared to be work to do rather than files to make.
#
.PHONY: hello hellog

hello: $(CLIBSD) hello/hello.c
	$(CC) $(CFLAGS) hello/hello.c $(CLIBS) -o bin/hello

hellog: $(GLIBSD) hello/hello.c
	$(CC) $(CFLAGS) hello/hello.c $(GLIBS) -o bin/hellog

#
# Snake game
#
# Snake is console only, but has cp and cpp versions as well.
#	
snake: $(CLIBSD) terminal_games/snake.c
	$(CC) $(CFLAGS) terminal_games/snake.c $(CLIBS) -o bin/snake

snake+: $(CLIBSCPPD) terminal_games/snake.cp
	$(CPP) $(CFLAGSCPP) terminal_games/snake.cp $(CLIBSCPP) -o bin/snake

snake++: $(CLIBSCPPD) terminal_games/snake.cpp
	$(CPP) $(CFLAGSCPP) terminal_games/snake.cpp $(CLIBSCPP) -o bin/snake

winobj: $(GLIBSD) tests/winobj.cpp
	$(CPP) $(CFLAGSCPP) tests/winobj.cpp $(GLIBS) -o bin/winobj
	
snakeg: $(GLIBSD) terminal_games/snake.c
	$(CC) $(CFLAGS) terminal_games/snake.c $(GLIBS) -o bin/snakeg

#
# Mine game
#	
mine: $(CLIBSD) terminal_games/mine.c
	$(CC) $(CFLAGS) terminal_games/mine.c $(CLIBS) -o bin/mine
	
mineg: $(GLIBSD) terminal_games/mine.c
	$(CC) $(CFLAGS) terminal_games/mine.c $(GLIBS) -o bin/mineg

#
# Wator game/dazzler
#	
wator: $(CLIBSD) terminal_programs/wator.c
	$(CC) $(CFLAGS) terminal_programs/wator.c $(CLIBS) -o bin/wator
	
watorg: $(GLIBSD) terminal_programs/wator.c
	$(CC) $(CFLAGS) terminal_programs/wator.c $(GLIBS) -o bin/watorg

#
# Pong game
#
# Note pong is a different program in console vs. graphical mode, the graphical
# version takes full advantage of the graphical model.
#	
pong: $(CLIBSD) terminal_games/pong.c
	$(CC) $(CFLAGS) terminal_games/pong.c $(CLIBS) -o bin/pong
	
pongg: $(GLIBSD) graph_games/pong.c
	$(CC) $(CFLAGS) graph_games/pong.c $(GLIBS) -o bin/pongg

#
# Breakout game for graphics
#
breakout: $(CLIBSD) terminal_games/breakout.c
	$(CC) $(CFLAGS) terminal_games/breakout.c $(CLIBS) -o bin/breakout

#
# Breakout game, windowed character edition
#
breakoutw: $(LIBPFX)termc$(LIBEXT) terminal_games/breakoutw.c
	$(CC) $(CFLAGS) terminal_games/breakoutw.c $(CLIBSC) -o bin/breakoutw

#
# Breakout game, graphical
#
breakoutg: $(GLIBSD) graph_games/breakoutg.c
	$(CC) $(CFLAGS) graph_games/breakoutg.c $(GLIBS) -o bin/breakoutg

#
# Breakout game, windowed demonstrator edition
#
breakoutwg: $(GLIBSD) graph_games/breakoutwg.c
	$(CC) $(CFLAGS) graph_games/breakoutwg.c $(GLIBS) -o bin/breakoutwg

#
# Breakout run remotely: linked with graph_client, the display and the
# sound served by graph_server. Two editions, matching the two graphical
# ones: breakoutgr draws in its own window, breakoutwgr carries the menus
# and the help and about windows, which is the fuller exercise of the
# remote protocol.
#
ifeq ($(OSTYPE),Windows_NT)
# the client carries the sound API itself, so the sound module stays out
breakoutgr: graph_games/breakoutg.c portable/graph_client.o \
	stub/screen_capture_stub.o windows/services.o windows/network.o \
	utils/config.o utils/option.o windows/stdio.o
	$(CC) $(CFLAGS) graph_games/breakoutg.c portable/graph_client.o \
	    stub/screen_capture_stub.o windows/services.o windows/network.o \
	    utils/config.o utils/option.o windows/stdio.o \
	    -lwinmm -lssl -lcrypto -lws2_32 -lcrypt32 -o bin/breakoutgr
else
breakoutgr: graph_games/breakoutg.c portable/graph_client.o \
	stub/screen_capture_stub.o
	$(CC) $(CFLAGS) graph_games/breakoutg.c portable/graph_client.o \
	    stub/screen_capture_stub.o $(LINUXSTDIO) linux/services.o \
	    utils/config.o utils/option.o linux/network.o \
	    -lssl -lcrypto -lm -lpthread -o bin/breakoutgr
endif

ifeq ($(OSTYPE),Windows_NT)
# the client carries the sound API itself, so the sound module stays out
breakoutwgr: graph_games/breakoutwg.c portable/graph_client.o \
	stub/screen_capture_stub.o windows/services.o windows/network.o \
	utils/config.o utils/option.o windows/stdio.o
	$(CC) $(CFLAGS) graph_games/breakoutwg.c portable/graph_client.o \
	    stub/screen_capture_stub.o windows/services.o windows/network.o \
	    utils/config.o utils/option.o windows/stdio.o \
	    -lwinmm -lssl -lcrypto -lws2_32 -lcrypt32 -o bin/breakoutwgr
else
breakoutwgr: graph_games/breakoutwg.c portable/graph_client.o \
	stub/screen_capture_stub.o
	$(CC) $(CFLAGS) graph_games/breakoutwg.c portable/graph_client.o \
	    stub/screen_capture_stub.o $(LINUXSTDIO) linux/services.o \
	    utils/config.o utils/option.o linux/network.o \
	    -lssl -lcrypto -lm -lpthread -o bin/breakoutwgr
endif

#
# Text editor
#	
editor: $(CLIBSD) terminal_programs/editor.c
	$(CC) $(CFLAGS) terminal_programs/editor.c $(CLIBS) -o bin/editor
	
editorg: $(GLIBSD) terminal_programs/editor.c
	$(CC) $(CFLAGS) terminal_programs/editor.c $(GLIBS) -o bin/editorg

#
# Get html/https page
#	
getpage: $(PLIBSD) network_programs/getpage.c
	$(CC) $(CFLAGS) network_programs/getpage.c $(PLIBS) -o bin/getpage
	
getpageg: $(GLIBSD) network_programs/getpage.c
	$(CC) $(CFLAGS) network_programs/getpage.c $(GLIBS) -o bin/getpageg

# Mail reader. Graphical, and uses the network, so it takes the graphics
# bundle, which carries the network module with it.
mail: $(GLIBSD) graph_programs/mail.c portable/mailcore.c
	$(CC) $(CFLAGS) graph_programs/mail.c portable/mailcore.c $(GLIBS) \
		-o bin/mail

#
# Read email, in characters
#
# The character library with the window manager in it, since mailc is
# windows and widgets on a terminal.
mailc: $(LIBPFX)termc$(LIBEXT) terminal_programs/mailc.c portable/mailcore.c
	$(CC) $(CFLAGS) terminal_programs/mailc.c portable/mailcore.c $(CLIBSC) \
		-o bin/mailc

#
# Read email, in characters, on a graphical window: the same character
# program linked against the graphical library, which speaks the whole
# terminal API onto a window of its own.
#
mailcg: $(GLIBSD) terminal_programs/mailc.c portable/mailcore.c
	$(CC) $(CFLAGS) terminal_programs/mailc.c portable/mailcore.c $(GLIBS) \
		-o bin/mailcg

#
# Get remote email
#
getmail: $(PLIBSD) network_programs/getmail.c
	$(CC) $(CFLAGS) network_programs/getmail.c $(PLIBS) -o bin/getmail

getmailg: $(GLIBSD) network_programs/getmail.c
	$(CC) $(CFLAGS) network_programs/getmail.c $(GLIBS) -o bin/getmailg

#
# Fake email server
#
fakemail: $(PLIBSD) network_programs/fakemail.c
	$(CC) $(CFLAGS) network_programs/fakemail.c $(PLIBS) -o bin/fakemail

fakemailg: $(GLIBSD) network_programs/fakemail.c
	$(CC) $(CFLAGS) network_programs/fakemail.c $(GLIBS) -o bin/fakemailg

#
# Network connection program
#
connectnet: $(PLIBSD) network_programs/connectnet.c
	$(CC) $(CFLAGS) network_programs/connectnet.c $(PLIBS) -o bin/connectnet

connectnetg: $(GLIBSD) network_programs/connectnet.c
	$(CC) $(CFLAGS) network_programs/connectnet.c $(GLIBS) -o bin/connectnetg
	
#
# Gettysberg address server
#	
gettys: $(PLIBSD) network_programs/gettys.c
	$(CC) $(CFLAGS) network_programs/gettys.c $(PLIBS) -o bin/gettys
	
gettysg: $(GLIBSD) network_programs/gettys.c
	$(CC) $(CFLAGS) network_programs/gettys.c $(GLIBS) -o bin/gettysg

#
# Message based networking test client
#	
msgclient: $(PLIBSD) network_programs/msgclient.c
	$(CC) $(CFLAGS) network_programs/msgclient.c $(PLIBS) -o bin/msgclient
	
msgclientg: $(GLIBSD) network_programs/msgclient.c
	$(CC) $(CFLAGS) network_programs/msgclient.c $(GLIBS) -o bin/msgclientg

#
# Message based networking test server
#	
msgserver: $(PLIBSD) network_programs/msgserver.c
	$(CC) $(CFLAGS) network_programs/msgserver.c $(PLIBS) -o bin/msgserver
	
msgserverg: $(GLIBSD) network_programs/msgserver.c
	$(CC) $(CFLAGS) network_programs/msgserver.c $(GLIBS) -o bin/msgserverg
	
#
# Print TCPIP/TLS certificates
#
prtcertnet: $(PLIBSD) network_programs/prtcertnet.c
	$(CC) $(CFLAGS) network_programs/prtcertnet.c $(PLIBS) -o bin/prtcertnet

prtcertnetg: $(GLIBSD) network_programs/prtcertnet.c
	$(CC) $(CFLAGS) network_programs/prtcertnet.c $(GLIBS) -o bin/prtcertnetg

#
# Print message based certificates
#	
prtcertmsg: $(PLIBSD) network_programs/prtcertmsg.c
	$(CC) $(CFLAGS) network_programs/prtcertmsg.c $(PLIBS) -o bin/prtcertmsg
	
prtcertmsgg: $(GLIBSD) network_programs/prtcertmsg.c
	$(CC) $(CFLAGS) network_programs/prtcertmsg.c $(GLIBS) -o bin/prtcertmsgg

#
# This program is missing???? check the linux machine.
#	
listcertnet: $(PLIBSD) network_programs/listcertnet.c
	$(CC) $(CFLAGS) network_programs/listcertnet.c $(PLIBS) -o bin/listcertnet
	
listcertnetg: $(GLIBSD) network_programs/listcertnet.c
	$(CC) $(CFLAGS) network_programs/listcertnet.c $(GLIBS) -o bin/listcertnetg

#
# Print Petit-Ami configuration tree
#	
prtconfig: $(PLIBSD) utils/prtconfig.c
	$(CC) $(CFLAGS) utils/prtconfig.c $(PLIBS) -o bin/prtconfig
	
prtconfigg: $(GLIBSD) utils/prtconfig.c
	$(CC) $(CFLAGS) utils/prtconfig.c $(GLIBS) -o bin/prtconfigg
	
#
# Pixel set/reset dazzler
#
pixel: $(GLIBSD) graph_programs/pixel.c
	$(CC) $(CFLAGS) graph_programs/pixel.c $(GLIBS) -o bin/pixel

#
# Moving balls dazzlers
#	
ball1: $(GLIBSD) graph_programs/ball1.c
	$(CC) $(CFLAGS) graph_programs/ball1.c $(GLIBS) -o bin/ball1
	
ball2: $(GLIBSD) graph_programs/ball2.c
	$(CC) $(CFLAGS) graph_programs/ball2.c $(GLIBS) -o bin/ball2
	
ball3: $(GLIBSD) graph_programs/ball3.c
	$(CC) $(CFLAGS) graph_programs/ball3.c $(GLIBS) -o bin/ball3
	
ball4: $(GLIBSD) graph_programs/ball4.c
	$(CC) $(CFLAGS) graph_programs/ball4.c $(GLIBS) -o bin/ball4
	
ball5: $(GLIBSD) graph_programs/ball5.c
	$(CC) $(CFLAGS) graph_programs/ball5.c $(GLIBS) -o bin/ball5
	
ball6: $(GLIBSD) graph_programs/ball6.c
	$(CC) $(CFLAGS) graph_programs/ball6.c $(GLIBS) -o bin/ball6

#
# Spreadsheet: grid, formulas, menus, file dialogs
#
spreadsheet: $(GLIBSD) graph_programs/spreadsheet.c
	$(CC) $(CFLAGS) graph_programs/spreadsheet.c $(GLIBS) -lz -o bin/spreadsheet
	
#
# Moving lines dazzlers
#
line1: $(GLIBSD) graph_programs/line1.c
	$(CC) $(CFLAGS) graph_programs/line1.c $(GLIBS) -o bin/line1

line2: $(GLIBSD) graph_programs/line2.c
	$(CC) $(CFLAGS) graph_programs/line2.c $(GLIBS) -o bin/line2
	
line4: $(GLIBSD) graph_programs/line4.c
	$(CC) $(CFLAGS) graph_programs/line4.c $(GLIBS) -o bin/line4
	
line5: $(GLIBSD) graph_programs/line5.c
	$(CC) $(CFLAGS) graph_programs/line5.c $(GLIBS) -o bin/line5
	
#
# Frame buffer interface, and its standalone test
#
linux/framebuffer/framebuffer.o: linux/framebuffer/framebuffer.c \
	linux/framebuffer/framebuffer.h Makefile
	$(CC) $(CFLAGS) -c linux/framebuffer/framebuffer.c \
	    -o linux/framebuffer/framebuffer.o

fbtest: linux/framebuffer/fbtest.c linux/framebuffer/framebuffer.o Makefile
	$(CC) $(CFLAGS) linux/framebuffer/fbtest.c \
	    linux/framebuffer/framebuffer.o -lpthread -o bin/fbtest

#
# Resizable clock
#
clock: $(GLIBSD) graph_programs/clock.c
	$(CC) $(CFLAGS) graph_programs/clock.c $(GLIBS) -o bin/clock

#
# Calculator
#
calc: $(GLIBSD) graph_programs/calc.c
	$(CC) $(CFLAGS) graph_programs/calc.c $(GLIBS) -o bin/calc

#
# Chess game
#
chess: $(GLIBSD) graph_games/chess.c
	$(CC) $(CFLAGS) graph_games/chess.c $(GLIBS) -o bin/chess

#
# Checkers game
#
checkers: $(GLIBSD) graph_games/checkers.c
	$(CC) $(CFLAGS) graph_games/checkers.c $(GLIBS) -o bin/checkers

#
# Defenders game (Space Invaders style)
#
defenders: $(GLIBSD) graph_games/defenders.c
	$(CC) $(CFLAGS) graph_games/defenders.c $(GLIBS) -o bin/defenders

#
# Conquest game (Risk-style territory conquest)
#
conquest: $(GLIBSD) graph_games/conquest.c
	$(CC) $(CFLAGS) graph_games/conquest.c $(GLIBS) -o bin/conquest

#
# Backgammon game
#
backgammon: $(GLIBSD) graph_games/backgammon.c
	$(CC) $(CFLAGS) graph_games/backgammon.c $(GLIBS) -o bin/backgammon

################################################################################
#
# Clean target
#
################################################################################

#
# Target and option guide
#
help:
	@echo ""
	@echo "Petit-Ami build"
	@echo ""
	@echo "Principal targets:"
	@echo ""
	@echo "  all                 every program and test (the default)"
	@echo "  clean               remove built programs and objects"
	@echo "  help                this guide"
	@echo ""
	@echo "Tests, by model:"
	@echo ""
	@echo "  stdio_test          serial (plain) model"
	@echo "  services_test       operating system services"
	@echo "  network_test        network model"
	@echo "  sound_test          sound model (sound_testg: graphical)"
	@echo "  terminal_test       terminal model (terminal_testg, terminal_testc)"
	@echo "  graphics_test       graphical model; 'graphics_test bench' runs"
	@echo "                      the benchmark section alone"
	@echo "  widget_test         widget set (widget_testc: terminal)"
	@echo "  window_test     window management (window_testc)"
	@echo "  event/eventg        event diagnostics"
	@echo ""
	@echo "The Wayland backend pair (Linux; built regardless of session):"
	@echo ""
	@echo "  graphics_testw      graphics test on the Wayland backend"
	@echo "  random_test         random procedures on several threads, until cancelled"
	@echo "  widget_testw        widget test on the Wayland backend"
	@echo "  graphics_test_gtk   the GTK4/Cairo benchmark edition (libgtk-4-dev)"
	@echo ""
	@echo "Demos and games: snake mine wator pong breakout chess checkers"
	@echo "  backgammon defenders editor clock calc pixel ball1-6 line1-5"
	@echo "  (a trailing g names the graphical build of a terminal program)"
	@echo ""
	@echo "Network programs: getpage getmail fakemail gettys msgclient"
	@echo "  msgserver prtcertnet prtcertmsg listcertnet"
	@echo ""
	@echo "Remote display: graph_server (see the remote regressions)"
	@echo ""
	@echo "Options, given as make variables:"
	@echo ""
	@echo "  GRAPHICS_BACKEND=x11|wayland|fb"
	@echo "                      graphics backend for the graphical model."
	@echo "                      Defaults to the running session: wayland"
	@echo "                      when WAYLAND_DISPLAY is set, else x11."
	@echo "                      fb targets the Linux frame buffer (run"
	@echo "                      from a text console)"
	@echo "  PDISPLAY_OPT=...    optimizer for the Wayland rasterizer,"
	@echo "                      default -O3; empty for stepping, add"
	@echo "                      -march=native for the last measure"
	@echo "  LINK_TYPE=static|dynamic"
	@echo "                      library linkage, default static"
	@echo "  STDIO_SOURCE=stdio  stdio override source selection"
	@echo ""
	@echo "The Wayland test rig, given in the environment:"
	@echo ""
	@echo "  PD_DUMP=<prefix>    write every presented frame as a ppm"
	@echo "  PD_INPUT=<fifo>     inject input: key/keydown/keyup <xkeycode>,"
	@echo "                      move <x> <y>, btn <1|2|3> <x> <y>"
	@echo "  PD_NOBEAT=1         suppress the live publish beat (timing runs)"
	@echo "                      (the AMI_WL_* spellings are honored as well)"
	@echo ""

clean:
	rm -f lsalsadev alsaparms
	rm -f bin/dumpmidi bin/test bin/play bin/playg bin/keyboard bin/keyboardg
	rm -f bin/playmidi bin/playmidig bin/playwave bin/playwaveg bin/printdev
	rm -f bin/printdevg bin/connectmidi bin/connectmidig bin/connectwave
	rm -f bin/connectwaveg bin/random bin/randomg bin/genwave bin/genwaveg 
	rm -f bin/terminal_test bin/terminal_testg bin/graphics_test 
	rm -f bin/window_test bin/widget_test bin/sound_test bin/sound_testg
	rm -f bin/services_test bin/stdio_test bin/event bin/eventg bin/term bin/termg bin/snake
	rm -f bin/snakeg bin/mine bin/mineg bin/wator bin/watorg bin/pong bin/pongg
	rm -f bin/breakout bin/editor bin/editorg bin/getpage bin/getpageg 
	rm -f bin/getmail bin/getmailg bin/gettys bin/gettysg bin/msgclient 
	rm -f bin/msgclientg bin/msgserver bin/msgserverg bin/prtcertnet
	rm -f bin/prtcertnetg bin/prtcertmsg bin/prtcertmsgg bin/listcertnet 
	rm -f bin/listcertnetg bin/prtconfig bin/prtconfigg bin/pixel bin/ball1
	rm -f bin/ball2 bin/ball3 bin/ball4 bin/ball5 bin/ball6 bin/line1 bin/line2
	rm -f bin/line4 bin/line5
	rm -f bin/services_test1 bin/connectnet bin/connectnetg bin/clock bin/calc
	find . -name "*.o" -type f -delete
	rm -f lib/*.a
	rm -f lib/*.so
	rm -f bin/*.exe
	
################################################################################
#
# Make distribution/release
#
# Does not work, go up a directory and execute:
#
# tar --exclude=.* -czvf petit_ami_M.m.tar.gz petit_ami
#
# Where M.m is major minor version.
#
# The formula below places the .gz one directory up. I have not found a way to
# exclude the tar.gz itself.
#
################################################################################

#dist:
#	cd ..
#	rm -f petit_ami_*.tar.gz
#	echo "will execute: tar --exclude=.* --exclude=*.gz -czvf petit_ami_$(VERMAJOR).$(VERMINOR).tar.gz petit_ami"
#	tar --exclude=.* --exclude=*.gz -czvf petit_ami_$(VERMAJOR).$(VERMINOR).tar.gz petit_ami

#
# The frame buffer graphics module: the Petit-Ami graphics subset drawn
# straight onto /dev/fb0 through the framebuffer.h interface. Runs on a
# text console.
#
linux/framebuffer/graphics.o: linux/framebuffer/graphics.c \
	linux/framebuffer/framebuffer.h linux/system_event.h \
	include/graphics.h Makefile
	$(CC) $(CFLAGS) -Ilinux -c linux/framebuffer/graphics.c \
	    -o linux/framebuffer/graphics.o

# the mock frame buffer: framebuffer.h over plain memory, dumped to a
# .ppm at exit, so the module can run headless
linux/framebuffer/fbmock.o: linux/framebuffer/fbmock.c \
	linux/framebuffer/framebuffer.h Makefile
	$(CC) $(CFLAGS) -c linux/framebuffer/fbmock.c \
	    -o linux/framebuffer/fbmock.o

# the objects every frame buffer program links: the graphics module, the
# stdio interdiction, services, config and system events
FBGRAPH = linux/framebuffer/graphics.o linux/stdio.o linux/services.o \
	utils/config.o utils/option.o linux/system_event.o \
	linux/wayland/screen_capture.o
FBLIBS = -lfreetype -lfontconfig -lpng -lm -lpthread

# graphics test on the real frame buffer: run from a text console
graphics_testfb: tests/graphics_test.c tests/auto_event.o $(FBGRAPH) \
	linux/framebuffer/framebuffer.o
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o $(FBGRAPH) \
	    linux/framebuffer/framebuffer.o $(FBLIBS) -o bin/graphics_testfb

# graphics test on the mock frame buffer: runs anywhere, final screen
# lands in fbmock.ppm, captures in test_images.img
graphics_testfbm: tests/graphics_test.c tests/auto_event.o $(FBGRAPH) \
	linux/framebuffer/fbmock.o
	$(CC) $(CFLAGS) tests/graphics_test.c tests/auto_event.o $(FBGRAPH) \
	    linux/framebuffer/fbmock.o $(FBLIBS) -o bin/graphics_testfbm

# breakout on the frame buffer: run from a text console
breakoutgfb: graph_games/breakoutg.c $(FBGRAPH) \
	linux/framebuffer/framebuffer.o lib/sound.o
	$(CC) $(CFLAGS) graph_games/breakoutg.c $(FBGRAPH) \
	    linux/framebuffer/framebuffer.o lib/sound.o $(FBLIBS) \
	    -lasound -lfluidsynth -o bin/breakoutgfb

#
# The managed frame buffer graphics model: the plug-in stack
# framebuffer <- graphics <- windowg <- widgets. The graphics layer is
# drawing only; portable/windowg.c subdivides its surface into windows
# by overriding the API, and the portable widget packages stand on the
# windows.
#
portable/windowg.o: portable/windowg.c include/graphics.h Makefile
	$(CC) $(CFLAGS) -c portable/windowg.c -o portable/windowg.o

FBMGRAPH = linux/framebuffer/graphics.o portable/windowg.o

lib/graphfbm_core.o: $(CORE_COMMON) $(FBMGRAPH) linux/system_event.o \
	portable/widget_base.o portable/gnome_widgets.o portable/plasma_widgets.o \
	portable/pdfgraph.o \
	cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o cpp/graphics.o
	ld -r -o lib/graphfbm_core.o $(CORE_COMMON) $(FBMGRAPH) \
	    linux/system_event.o portable/widget_base.o portable/gnome_widgets.o \
	    portable/plasma_widgets.o portable/pdfgraph.o \
	    cpp/terminal.o cpp/sound.o cpp/services.o cpp/network.o cpp/graphics.o

lib/libami_graphfbm.a: lib/graphfbm_core.o lib/sound.o linux/network.o
	rm -f lib/libami_graphfbm.a
	ar rcs lib/libami_graphfbm.a lib/graphfbm_core.o lib/sound.o linux/network.o

# the backend archive proper: the same stack with the frame buffer
# device layer carried inside, so programs link it like any other
# graphical backend (GRAPHICS_BACKEND=fb)
lib/graphfb_core.o: lib/graphfbm_core.o linux/framebuffer/framebuffer.o
	ld -r -o lib/graphfb_core.o lib/graphfbm_core.o \
	    linux/framebuffer/framebuffer.o

lib/libami_graphfb.a: lib/graphfb_core.o lib/sound.o linux/network.o
	rm -f lib/libami_graphfb.a
	ar rcs lib/libami_graphfb.a lib/graphfb_core.o lib/sound.o linux/network.o

GLIBSFBM = stub/keeper.o lib/libami_graphfbm.a
FBMLIBS = -lasound -lfluidsynth -lssl -lcrypto -lstdc++ -lfreetype \
	-lfontconfig -lm -lpthread -lpng -lz

# real console editions
hellofb: $(GLIBSFBM) hello/hello.c linux/framebuffer/framebuffer.o
	$(CC) $(CFLAGS) hello/hello.c linux/framebuffer/framebuffer.o \
	    $(GLIBSFBM) $(FBMLIBS) -o bin/hellofb

widget_testfb: $(GLIBSFBM) tests/widget_test.c linux/framebuffer/framebuffer.o \
	linux/wayland/screen_capture.o
	$(CC) $(CFLAGS) tests/widget_test.c linux/wayland/screen_capture.o \
	    linux/framebuffer/framebuffer.o \
	    $(GLIBSFBM) $(FBMLIBS) -o bin/widget_testfb

window_testfb: $(GLIBSFBM) tests/window_test.c tests/auto_event.o \
	linux/framebuffer/framebuffer.o linux/wayland/screen_capture.o
	$(CC) $(CFLAGS) tests/window_test.c tests/auto_event.o linux/wayland/screen_capture.o \
	    linux/framebuffer/framebuffer.o \
	    $(GLIBSFBM) $(FBMLIBS) -o bin/window_testfb

# mock editions, for the rig
hellofbm: $(GLIBSFBM) hello/hello.c linux/framebuffer/fbmock.o
	$(CC) $(CFLAGS) hello/hello.c linux/framebuffer/fbmock.o \
	    $(GLIBSFBM) $(FBMLIBS) -o bin/hellofbm

widget_testfbmk: $(GLIBSFBM) tests/widget_test.c linux/framebuffer/fbmock.o \
	linux/wayland/screen_capture.o
	$(CC) $(CFLAGS) tests/widget_test.c linux/wayland/screen_capture.o \
	    linux/framebuffer/fbmock.o \
	    $(GLIBSFBM) $(FBMLIBS) -o bin/widget_testfbmk

window_testfbm: $(GLIBSFBM) tests/window_test.c tests/auto_event.o \
	linux/framebuffer/fbmock.o linux/wayland/screen_capture.o
	$(CC) $(CFLAGS) tests/window_test.c tests/auto_event.o linux/wayland/screen_capture.o \
	    linux/framebuffer/fbmock.o \
	    $(GLIBSFBM) $(FBMLIBS) -o bin/window_testfbm
