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
        # glibc assumes that this is a patched glibc with override calls.
        #
        #STDIO_SOURCE=glibc
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
        ifeq ($(STDIO_SOURCE),stdio)
            # Linking local stdio, must be dynamic
            LINK_TYPE=dynamic
        else
            # LINK_TYPE=static
            LINK_TYPE=dynamic
        endif
        
    endif
    
endif

#
# Does terminal model get window management?
#
ifndef USEMANAGERC

	#
	# Default is managerc disabled on terminal mode
	#
	USEMANAGERC=0

endif

#
# Set managerc object
#
ifeq ($(USEMANAGERC),1)

	MANAGERC=portable/managerc.o

else

	MANAGERC=

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
	CFLAGS=-g3 -Iinclude $(SSL_CFLAGS) -I$(FT_PREFIX)/include/freetype2 -I/opt/X11/include

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
    ifneq ($(STDIO_SOURCE),stdio)
	# Link path is through bin to get glibc.
	#
    CFLAGS+=-Wl,--rpath=bin
    endif
    # FreeType/fontconfig for font rendering
    CFLAGS+=$(shell pkg-config --cflags freetype2 fontconfig)
    
endif

#
# Set library dependencies
#
ifeq ($(LINK_TYPE),static)
    LIBEXT = .a
else
    LIBEXT = .so
endif

#
# Select where stdio.h comes from
#
ifeq ($(STDIO_SOURCE),stdio)
    #
    # In local link, we need to get stdio.h from local directory
    #
    CFLAGS +=-Ilibc
    ifneq ($(OSTYPE),Windows_NT)
    ifneq ($(OSTYPE),Darwin)
        # Linux and FreeBSD use X11/libXau which accesses FILE* internals
        # directly (backdoor access). STDIO_BYPASS prevents petit_ami from
        # exporting fopen/fclose etc. as global symbols, so libXau always
        # uses the real system fopen and a proper FILE*. Mac OS X is exempt
        # because it uses Cocoa/CoreGraphics and does not involve libXau.
        CFLAGS += -DSTDIO_BYPASS
    endif
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

        # Linux
        CFLAGS += -static
        
    endif
    
endif

#
# No reason at present for CPP to be different than C
#
CFLAGSCPP = $(CFLAGS)

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

	ifeq ($(STDIO_SOURCE),stdio)
	
		# Nothing, libc is linked in overall lib
		
	else
	
	    #
	    # Linux, use modified GLIBC
	    #
	    
	    #
	    # For modified GLIBC, we need to specify the libary first or it won't
	    # link correctly.
	    #
	    ifeq ($(LINK_TYPE),static)
	        PLIBS = -Wl,--whole-archive bin/libc.a -Wl,--no-whole-archive
	        CLIBS = -Wl,--whole-archive bin/libc.a -Wl,--no-whole-archive
	        GLIBS = -Wl,--whole-archive bin/libc.a -Wl,--no-whole-archive
	        PLIBSD = bin/libc.a
	        CLIBSD = bin/libc.a
	        GLIBSD = bin/libc.a
	    else
		    PLIBS = bin/libc.so.6
		    CLIBS = bin/libc.so.6
		    GLIBS = bin/libc.so.6
	    endif
	   
	endif
    
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
    	PLIBS += lib/petit_ami_plain.a
    else ifeq ($(OSTYPE),FreeBSD)
    	PLIBS += lib/petit_ami_plain.a
    else
    	PLIBS += -Wl,--whole-archive lib/petit_ami_plain.a -Wl,--no-whole-archive
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
    	CLIBS += lib/petit_ami_term.a
    else ifeq ($(OSTYPE),FreeBSD)
    	CLIBS += lib/petit_ami_term.a
    else
    	CLIBS += -Wl,--whole-archive lib/petit_ami_term.a -Wl,--no-whole-archive
    endif
else
    CLIBS += stub/keeper.o lib/petit_ami_term.so 
endif

CLIBSCPP = $(CLIBS) cpp/terminal.o

#
# Graphical model API
#
ifeq ($(LINK_TYPE),static)
    ifeq ($(OSTYPE),Darwin)
    	GLIBS += -Wl,-force_load,lib/petit_ami_graph.a
    else
    	GLIBS += -Wl,--whole-archive lib/petit_ami_graph.a -Wl,--no-whole-archive
    endif
else
    GLIBS += stub/keeper.o lib/petit_ami_graph.so
endif

#
# Create dependency macros
#
PLIBSD += lib/petit_ami_plain$(LIBEXT)
CLIBSD += lib/petit_ami_term$(LIBEXT) stub/keeper.o
GLIBSD += lib/petit_ami_graph$(LIBEXT) stub/keeper.o

CLIBSCPPD = $(CLIBSD) cpp/terminal.o
#
# add external packages
#
ifeq ($(OSTYPE),Windows_NT)

    #
    # Windows
    #
    PLIBS += -lwinmm -lwsock32
    CLIBS += -lwinmm -lgdi32 -lwsock32
    GLIBS += -lwinmm -lgdi32 -lcomdlg32 -lwsock32

else ifeq ($(OSTYPE),Darwin)

    #
    # Mac OS X
    #
    PLIBS += $(SSL_LIBS)
    CLIBS += $(SSL_LIBS)
    GLIBS += $(SSL_LIBS) \
             -framework Cocoa \
             -framework CoreGraphics \
             -framework CoreText \
             -framework CoreFoundation \
             -framework ImageIO

else ifeq ($(OSTYPE),FreeBSD)

    #
    # BSD
    #
	PLIBS += -L/usr/local/lib -lm -lpthread -lssl -lcrypto
	CLIBS += -L/usr/local/lib -lm -lpthread -lssl -lcrypto
	GLIBS += -L/usr/local/lib -lm -lpthread -lssl -lcrypto -lX11 \
	         -lfreetype -lfontconfig
	PLIBSD +=
    CLIBSD +=
	GLIBSD +=

else

    #
    # Linux
    #
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
all: dumpmidi play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testg \
     graphics_test testviewer management_test widget_test \
     sound_test sound_testg services_test event eventg term termg snake snakeg mine mineg \
     wator watorg pong pongg breakout backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 line2 \
     line4 line5 clock calc

else ifeq ($(OSTYPE),Darwin)

#
# Mac OS X
#
all: dumpmidi play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testg \
     graphics_test testviewer management_test widget_test \
     sound_test sound_testg services_test event eventg term termg snake snakeg mine mineg \
     wator watorg pong pongg breakout backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock calc

else ifeq ($(OSTYPE),FreeBSD)

#
# BSD
#
all: dumpmidi play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testg \
     graphics_test testviewer management_test widget_test \
     sound_test sound_testg services_test event eventg term termg snake snakeg mine mineg \
     wator watorg pong pongg breakout backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock calc
    
else

#
# Linux
#
all: dumpmidi play playg keyboard keyboardg playmidi playmidig playwave \
     playwaveg printdev printdevg connectmidi connectmidig connectwave \
     connectwaveg random randomg genwave genwaveg terminal_test terminal_testg \
     graphics_test testviewer management_test widget_test \
     sound_test sound_testg services_test event eventg term termg snake snakeg mine mineg \
     wator watorg pong pongg breakout backgammon checkers chess defenders editor editorg getpage getpageg getmail \
     getmailg fakemail gettys gettysg msgclient msgclientg msgserver msgserverg \
     prtcertnet prtcertnetg prtcertmsg prtcertmsgg listcertnet listcertnetg \
     prtconfig prtconfigg pixel ball1 ball2 ball3 ball4 ball5 ball6 line1 \
     line2 line4 line5 clock calc
    
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
	$(CC) $(CFLAGS) -c libc/stdio.c -o linux/stdio.o

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
	
linux/graphics.o: linux/graphics.c include/graphics.h Makefile
	$(CC) $(CFLAGS) -c linux/graphics.c -o linux/graphics.o
	
linux/system_event.o: linux/system_event.c linux/system_event.h Makefile
	$(CC) $(CFLAGS) -c linux/system_event.c -o linux/system_event.o
	

linux/screen_capture.o: linux/screen_capture.c Makefile
	$(CC) $(CFLAGS) -c linux/screen_capture.c -o linux/screen_capture.o
	
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
	
macosx/sound.o: stub/sound.c include/sound.h Makefile
	$(CC) $(CFLAGS) -c stub/sound.c -o macosx/sound.o
	
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

bsd/sound.o: stub/sound.c include/sound.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c stub/sound.c -o bsd/sound.o

bsd/network.o: stub/network.c include/network.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I/usr/local/include \
		-c stub/network.c -o bsd/network.o

bsd/terminal.o: linux/terminal.c include/terminal.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c linux/terminal.c -o bsd/terminal.o

bsd/graphics.o: linux/graphics.c include/graphics.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I/usr/local/include -c linux/graphics.c \
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
	
portable/gnome_widgets.o: portable/gnome_widgets.c
	$(CC) $(CFLAGS) -c portable/gnome_widgets.c \
		-o portable/gnome_widgets.o
		
portable/managerc.o: portable/managerc.c
	$(CC) $(CFLAGS) -c portable/managerc.c \
		-o portable/managerc.o
	
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
lib/petit_ami_plain.a: windows/services.o windows/sound.o windows/network.o \
	utils/option.o utils/config.o windows/stdio.o
	ar rcs lib/petit_ami_plain.a windows/services.o windows/sound.o \
        windows/network.o utils/config.o utils/option.o windows/stdio.o
	
lib/petit_ami_term.a: windows/services.o windows/sound.o windows/network.o \
    windows/terminal.o utils/config.o utils/option.o windows/stdio.o
	ar rcs lib/petit_ami_term.a windows/services.o windows/sound.o \
	    windows/network.o windows/terminal.o utils/config.o utils/option.o \
	    windows/stdio.o
	
lib/petit_ami_graph.a: windows/services.o windows/sound.o windows/network.o \
    windows/graphics.o utils/config.o utils/option.o windows/stdio.o
	ar rcs lib/petit_ami_graph.a windows/services.o windows/sound.o \
	    windows/network.o windows/graphics.o utils/config.o utils/option.o \
	    windows/stdio.o
	
else ifeq ($(OSTYPE),Darwin)

#
# Mac OS X
#
# Mac OS X cannot use .so files, but rather uses statically linked files.
#
lib/petit_ami_plain.a: macosx/services.o macosx/sound.o macosx/network.o \
	utils/config.o utils/option.o macosx/stdio.o
	ar rcs lib/petit_ami_plain.a macosx/services.o macosx/sound.o \
        macosx/network.o utils/config.o utils/option.o macosx/stdio.o
	
lib/petit_ami_term.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/system_event.o macosx/terminal.o utils/config.o utils/option.o \
    macosx/stdio.o
	ar rcs lib/petit_ami_term.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/system_event.o macosx/terminal.o \
	    utils/config.o utils/option.o macosx/stdio.o
	
lib/petit_ami_graph.a: macosx/services.o macosx/sound.o macosx/network.o \
    macosx/system_event.o macosx/graphics.o macosx/graphics_cocoa.o \
    utils/config.o utils/option.o macosx/stdio.o
	ar rcs lib/petit_ami_graph.a macosx/services.o macosx/sound.o \
	    macosx/network.o macosx/system_event.o macosx/graphics.o \
	    macosx/graphics_cocoa.o utils/config.o utils/option.o macosx/stdio.o

else ifeq ($(OSTYPE),FreeBSD)

#
# BSD
#
# Use statically linked files, for BSD
#
lib/petit_ami_plain.a: bsd/services.o bsd/sound.o bsd/network.o \
	utils/config.o utils/option.o bsd/stdio.o
	ar rcs lib/petit_ami_plain.a bsd/services.o bsd/sound.o \
        bsd/network.o utils/config.o utils/option.o bsd/stdio.o
	
lib/petit_ami_term.a: bsd/services.o bsd/sound.o bsd/network.o \
    bsd/system_event.o bsd/terminal.o utils/config.o utils/option.o \
    bsd/stdio.o
	ar rcs lib/petit_ami_term.a bsd/services.o bsd/sound.o \
	    bsd/network.o bsd/system_event.o bsd/terminal.o \
	    utils/config.o utils/option.o bsd/stdio.o
	
lib/petit_ami_graph.a: bsd/services.o bsd/sound.o bsd/network.o \
    bsd/graphics.o bsd/system_event.o \
	portable/gnome_widgets.o utils/config.o utils/option.o bsd/stdio.o
	ar rcs lib/petit_ami_graph.a bsd/services.o bsd/sound.o \
	    bsd/network.o bsd/system_event.o bsd/graphics.o \
	    portable/gnome_widgets.o utils/config.o utils/option.o bsd/stdio.o
	
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
	
lib/petit_ami_plain.a: $(LINUXSTDIO) linux/services.o linux/sound.o \
	linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
	utils/config.o utils/option.o
	ar rcs lib/petit_ami_plain.a $(LINUXSTDIO) linux/services.o linux/sound.o \
	    linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
	    utils/config.o utils/option.o
	
lib/petit_ami_term.so: $(LINUXSTDIO) linux/services.o linux/network.o \
	linux/terminal.o $(MANAGERC) linux/system_event.o utils/config.o utils/option.o \
    cpp/terminal.o
	$(CC) -shared $(LINUXSTDIO) linux/services.o linux/network.o \
		linux/terminal.o $(MANAGERC) linux/system_event.o utils/config.o \
		utils/option.o  cpp/terminal.o -o lib/petit_ami_term.so 
	
lib/petit_ami_term.a: $(LINUXSTDIO) linux/services.o linux/sound.o \
	linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
	linux/terminal.o $(MANAGERC) linux/system_event.o utils/config.o utils/option.o \
    cpp/terminal.o
	ar rcs lib/petit_ami_term.a $(LINUXSTDIO) linux/services.o linux/sound.o \
		linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
		linux/terminal.o $(MANAGERC) linux/system_event.o utils/config.o utils/option.o \
		 cpp/terminal.o
	
lib/petit_ami_graph.so: $(LINUXSTDIO) linux/services.o linux/network.o \
	linux/graphics.o linux/system_event.o \
	portable/gnome_widgets.o utils/config.o utils/option.o cpp/terminal.o
	$(CC) -shared $(LINUXSTDIO) linux/services.o linux/network.o \
		linux/graphics.o linux/system_event.o \
		portable/gnome_widgets.o utils/config.o utils/option.o cpp/terminal.o \
        -o lib/petit_ami_graph.so

lib/petit_ami_graph.a: $(LINUXSTDIO) linux/services.o linux/sound.o \
	linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
	linux/graphics.o linux/system_event.o \
	portable/gnome_widgets.o utils/config.o utils/option.o cpp/terminal.o
	ar rcs lib/petit_ami_graph.a $(LINUXSTDIO) linux/services.o linux/sound.o \
		linux/fluidsynthplug.o linux/dumpsynthplug.o linux/network.o \
		linux/graphics.o linux/system_event.o \
		portable/gnome_widgets.o utils/config.o utils/option.o  \
		cpp/terminal.o
	
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
	$(CC) $(CFLAGS) test.c $(GLIBS) -o testg
	
test+: $(PLIBSD) test.cp
	$(CPP) $(CFLAGS) test.cp $(PLIBS) -o test
	
test++: $(PLIBSD) test.cpp
	$(CPP) $(CFLAGS) test.cpp $(PLIBS) -o test
	
testc+: $(CLIBSCPPD) test.cp
	$(CPP) $(CFLAGSCPP) test.cp $(CLIBSCPP) -o testc
	
testc++: $(CLIBSCPPD) test.cpp
	$(CPP) $(CFLAGSCPP) test.cpp $(CLIBSCPP) -o testc
	
testg++: $(GLIBSD) test.cpp
	$(CPP) $(CFLAGS) test.cpp $(GLIBS) -o testg
	
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
ifeq ($(OSTYPE),Windows_NT)
SCREEN_CAPTURE_OBJ = windows/screen_capture.o
else ifeq ($(OSTYPE),Darwin)
SCREEN_CAPTURE_OBJ = macosx/screen_capture.o
else ifeq ($(OSTYPE),FreeBSD)
SCREEN_CAPTURE_OBJ = bsd/screen_capture.o
else
SCREEN_CAPTURE_OBJ = linux/screen_capture.o
endif

#
# Test console model compliant output
#
ifeq ($(OSTYPE),Darwin)
terminal_test: $(CLIBSD) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ) $(CLIBS) -o bin/terminal_test
else
terminal_test: $(CLIBSD) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ) $(CLIBS) -lX11 -lpng -lz -o bin/terminal_test
endif

ifeq ($(OSTYPE),Darwin)
terminal_testg: $(GLIBSD) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/terminal_testg
else
terminal_testg: $(GLIBSD) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/terminal_test.c $(SCREEN_CAPTURE_OBJ) $(GLIBS) -lpng -lz -o bin/terminal_testg
endif

#
# Test graph model compliant output
#
ifeq ($(OSTYPE),Darwin)
graphics_test: $(GLIBSD) tests/graphics_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/graphics_test.c $(SCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/graphics_test
else
graphics_test: $(GLIBSD) tests/graphics_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/graphics_test.c $(SCREEN_CAPTURE_OBJ) $(GLIBS) -lpng -lz -o bin/graphics_test
endif

#
# BMP-stream frame viewer: walks the test_images file produced by
# screen_capture, one frame per keypress (left/right arrows).
#
ifeq ($(OSTYPE),Windows_NT)
testviewer: windows/testviewer.c Makefile
	$(CC) -g3 windows/testviewer.c -lpng -lz -lgdi32 -o bin/testviewer
else ifeq ($(OSTYPE),Darwin)
testviewer: macosx/testviewer.c Makefile
	$(CC) -g3 -x objective-c -fobjc-arc macosx/testviewer.c \
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
# Test windows management model compliant output
#
ifeq ($(OSTYPE),Darwin)
management_test: $(GLIBSD) tests/management_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/management_test.c $(SCREEN_CAPTURE_OBJ) $(GLIBS) -o bin/management_test
else
management_test: $(GLIBSD) tests/management_test.c $(SCREEN_CAPTURE_OBJ)
	$(CC) $(CFLAGS) tests/management_test.c $(SCREEN_CAPTURE_OBJ) $(GLIBS) -lpng -lz -o bin/management_test
endif

#
# Test windows widget compliant output
#
widget_test: $(GLIBSD) tests/widget_test.c
	$(CC) $(CFLAGS) tests/widget_test.c $(GLIBS) -o bin/widget_test 
	
#
# Test sound model compliant input/output (uses console timers)
#	
sound_test: $(CLIBSD) tests/sound_test.c
	$(CC) $(CFLAGS) tests/sound_test.c $(CLIBS) -o bin/sound_test 
	
sound_testg: $(GLIBSD) tests/sound_test.c
	$(CC) $(CFLAGS) tests/sound_test.c $(GLIBS) -o bin/sound_testg 
	
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
# Test event model (console and graph mode)
#	
event: $(CLIBSD) tests/event.c
	$(CC) $(CFLAGS) tests/event.c $(CLIBS) -o bin/event
	
eventg: $(GLIBSD) tests/event.c
	$(CC) $(CFLAGS) tests/event.c $(GLIBS) -o bin/eventg

#
# Test terminal characteristics (console and graph mode)
#	
term: $(CLIBSD) tests/term.c
	$(CC) $(CFLAGS) tests/term.c $(CLIBS) -o bin/term
	
termg: $(GLIBSD) tests/term.c
	$(CC) $(CFLAGS) tests/term.c $(GLIBS) -o bin/termg
	
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
breakout: $(GLIBSD) graph_games/breakout.c
	$(CC) $(CFLAGS) graph_games/breakout.c $(GLIBS) -o bin/breakout

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

clean:
	rm -f lsalsadev alsaparms
	rm -f bin/dumpmidi bin/test bin/play bin/playg bin/keyboard bin/keyboardg
	rm -f bin/playmidi bin/playmidig bin/playwave bin/playwaveg bin/printdev
	rm -f bin/printdevg bin/connectmidi bin/connectmidig bin/connectwave
	rm -f bin/connectwaveg bin/random bin/randomg bin/genwave bin/genwaveg 
	rm -f bin/terminal_test bin/terminal_testg bin/graphics_test 
	rm -f bin/management_test bin/widget_test bin/sound_test bin/sound_testg
	rm -f bin/services_test bin/event bin/eventg bin/term bin/termg bin/snake 
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
