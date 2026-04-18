#
# Makefile for BSD curses games and nano on Petit-Ami
#
# Builds each program in both terminal and graphics versions using the
# curses compatibility layer in apis/curses.c.
#
# Usage: make               (builds all terminal + graphical variants)
#        make nano nanog    (builds one program, terminal + graphical)
#        make clean
#
# Naming convention (Ami standard):
#   <name>   terminal build (links terminal.c)
#   <name>g  graphical build (links graphics.c)
#
# Binaries and .adp adapter files are installed into $(BINDIR) so the
# curses adapter (ami_getpgm) finds <progname>.adp alongside the binary.
#

ROOT     = ..
BINDIR   = $(ROOT)/bin
CC       = gcc

# flags for building the curses adapter (needs Ami stdio interception)
AMIFLAGS = -g3 -I$(ROOT)/apis -I$(ROOT)/include -I$(ROOT)/libc \
           -I/usr/include/freetype2 -I/usr/include/libpng16 -DSTDIO_BYPASS

# flags for building program source (standard libc, no Ami stdio override)
CFLAGS   = -g3 -I$(ROOT)/apis -D'__COPYRIGHT(x)=' -D'__RCSID(x)='

CURSES_TOBJ = $(ROOT)/apis/curses.o
CURSES_GOBJ = $(ROOT)/apis/cursesg.o

# Static link against Petit-Ami .o files directly.
TOBJ    = $(ROOT)/linux/stdio.o $(ROOT)/linux/services.o \
          $(ROOT)/linux/network.o $(ROOT)/linux/terminal.o \
          $(ROOT)/linux/system_event.o \
          $(ROOT)/utils/config.o $(ROOT)/utils/option.o \
          $(ROOT)/cpp/terminal.o

GOBJ    = $(ROOT)/linux/stdio.o $(ROOT)/linux/services.o \
          $(ROOT)/linux/network.o $(ROOT)/linux/graphics.o \
          $(ROOT)/linux/system_event.o \
          $(ROOT)/portable/gnome_widgets.o \
          $(ROOT)/utils/config.o $(ROOT)/utils/option.o \
          $(ROOT)/cpp/terminal.o

TLIBS   = $(CURSES_TOBJ) $(ROOT)/stub/keeper.o $(TOBJ) \
          $(ROOT)/linux/sound.o $(ROOT)/linux/fluidsynthplug.o \
          $(ROOT)/linux/dumpsynthplug.o \
          -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto -lbsd -lstdc++

GLIBS   = $(CURSES_GOBJ) $(ROOT)/stub/keeper.o $(GOBJ) \
          $(ROOT)/linux/sound.o $(ROOT)/linux/fluidsynthplug.o \
          $(ROOT)/linux/dumpsynthplug.o \
          -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto \
          -lX11 -lXext -lfreetype -lfontconfig -lbsd -lstdc++

GAMES     = snake rain worm worms canfield
PROGRAMS  = $(GAMES) nano
# Standalone programs: have their own build system, terminal-only (no graphical
# variant today), built by this Makefile as a per-target recipe.
STANDALONE = htop
BINS      = $(PROGRAMS) $(addsuffix g,$(PROGRAMS)) $(STANDALONE)
BIN_EXES  = $(addprefix $(BINDIR)/,$(BINS))

# Any .adp files in this directory are staged next to the binaries.
ADP_SRC  = $(wildcard *.adp)
BIN_ADP  = $(addprefix $(BINDIR)/,$(ADP_SRC))

all: $(CURSES_TOBJ) $(CURSES_GOBJ) $(BIN_EXES) $(BIN_ADP)

# Short phony aliases: `make nano` builds $(BINDIR)/nano.
.PHONY: all clean $(BINS)
$(foreach b,$(BINS),$(eval $(b): $(BINDIR)/$(b) $(BIN_ADP)))

# curses.o — linked with terminal.c; menu code compiled out.
$(CURSES_TOBJ): $(ROOT)/apis/curses.c $(ROOT)/apis/curses.h
	$(CC) $(AMIFLAGS) -c $(ROOT)/apis/curses.c -o $(CURSES_TOBJ)

# cursesg.o — linked with graphics.c; menu code enabled via AMI_CURSES_MENU.
$(CURSES_GOBJ): $(ROOT)/apis/curses.c $(ROOT)/apis/curses.h
	$(CC) $(AMIFLAGS) -DAMI_CURSES_MENU -c $(ROOT)/apis/curses.c -o $(CURSES_GOBJ)

# Copy any .adp adapter file from the source dir to $(BINDIR).
$(BINDIR)/%.adp: %.adp | $(BINDIR)
	cp $< $@

$(BINDIR):
	mkdir -p $(BINDIR)

# Snake
SNAKE_SRC   = BSDGames/snake/snake/snake.c
SNAKE_FLAGS = -IBSDGames/snake/snake
$(BINDIR)/snake:  $(CURSES_TOBJ) $(SNAKE_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(SNAKE_FLAGS) -o $@ $(SNAKE_SRC) $(TLIBS) -lm
$(BINDIR)/snakeg: $(CURSES_GOBJ) $(SNAKE_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(SNAKE_FLAGS) -o $@ $(SNAKE_SRC) $(GLIBS) -lm

# Rain
RAIN_SRC = BSDGames/rain/rain.c
$(BINDIR)/rain:  $(CURSES_TOBJ) $(RAIN_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(RAIN_SRC) $(TLIBS)
$(BINDIR)/raing: $(CURSES_GOBJ) $(RAIN_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(RAIN_SRC) $(GLIBS)

# Worm
WORM_SRC = BSDGames/worm/worm.c
$(BINDIR)/worm:  $(CURSES_TOBJ) $(WORM_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(WORM_SRC) $(TLIBS)
$(BINDIR)/wormg: $(CURSES_GOBJ) $(WORM_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(WORM_SRC) $(GLIBS)

# Worms
WORMS_SRC = BSDGames/worms/worms.c
$(BINDIR)/worms:  $(CURSES_TOBJ) $(WORMS_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(WORMS_SRC) $(TLIBS)
$(BINDIR)/wormsg: $(CURSES_GOBJ) $(WORMS_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(WORMS_SRC) $(GLIBS)

# Canfield
CANFIELD_SRC   = BSDGames/canfield/canfield/canfield.c
CANFIELD_FLAGS = -IBSDGames/canfield/canfield
$(BINDIR)/canfield:  $(CURSES_TOBJ) $(CANFIELD_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(CANFIELD_FLAGS) -o $@ $(CANFIELD_SRC) $(TLIBS)
$(BINDIR)/canfieldg: $(CURSES_GOBJ) $(CANFIELD_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(CANFIELD_FLAGS) -o $@ $(CANFIELD_SRC) $(GLIBS)

# Gomoku (not in `all`, build explicitly)
GOMOKU_SRC   = BSDGames/gomoku/main.c BSDGames/gomoku/bdisp.c \
               BSDGames/gomoku/stoc.c BSDGames/gomoku/makemove.c \
               BSDGames/gomoku/pickmove.c BSDGames/gomoku/bdinit.c
GOMOKU_FLAGS = -IBSDGames/gomoku -I$(ROOT)/apis/bsdcompat -Dgetline=gomoku_getline
$(BINDIR)/gomoku:  $(CURSES_TOBJ) $(GOMOKU_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(GOMOKU_FLAGS) -o $@ $(GOMOKU_SRC) $(TLIBS)
$(BINDIR)/gomokug: $(CURSES_GOBJ) $(GOMOKU_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(GOMOKU_FLAGS) -o $@ $(GOMOKU_SRC) $(GLIBS)
.PHONY: gomoku gomokug
gomoku:  $(BINDIR)/gomoku  $(BIN_ADP)
gomokug: $(BINDIR)/gomokug $(BIN_ADP)

# Nano (v1.2.5)
NANO_SRC   = nano/nano.c nano/winio.c nano/files.c nano/cut.c \
             nano/search.c nano/move.c nano/color.c nano/global.c \
             nano/rcfile.c nano/utils.c
NANO_FLAGS = -Inano -DNANO_SMALL
$(BINDIR)/nano:  $(CURSES_TOBJ) $(NANO_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(NANO_FLAGS) -o $@ $(NANO_SRC) $(TLIBS)
$(BINDIR)/nanog: $(CURSES_GOBJ) $(NANO_SRC) | $(BINDIR)
	$(CC) $(CFLAGS) $(NANO_FLAGS) -o $@ $(NANO_SRC) $(GLIBS)

# htop (terminal only — uses its own autoconf+automake build).
# Overrides LIBS / AM_CPPFLAGS to swap ncursesw for the Petit-Ami stack.
# Configure runs once (autogen then configure) per fresh tree; thereafter
# htop's own Makefile handles incremental rebuilds. The $(CURSES_TOBJ)
# prerequisite forces a re-link if the adapter changes.
HTOP_DIR      = htop
HTOP_ROOT     = $(abspath $(ROOT))
HTOP_PA_LIBS  = $(HTOP_ROOT)/apis/curses.o $(HTOP_ROOT)/stub/keeper.o \
                $(HTOP_ROOT)/linux/stdio.o $(HTOP_ROOT)/linux/services.o \
                $(HTOP_ROOT)/linux/network.o $(HTOP_ROOT)/linux/terminal.o \
                $(HTOP_ROOT)/linux/system_event.o \
                $(HTOP_ROOT)/utils/config.o $(HTOP_ROOT)/utils/option.o \
                $(HTOP_ROOT)/cpp/terminal.o \
                $(HTOP_ROOT)/linux/sound.o $(HTOP_ROOT)/linux/fluidsynthplug.o \
                $(HTOP_ROOT)/linux/dumpsynthplug.o \
                -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto -lbsd -lstdc++
HTOP_PA_CPPFLAGS = -I$(HTOP_ROOT)/apis -I$(HTOP_ROOT)/include \
                   -I/usr/include/libunwind -DNDEBUG
HTOP_LIBS_OVERRIDE = -lunwind -Wl,-Bsymbolic-functions $(HTOP_PA_LIBS) -ldl -lm

$(HTOP_DIR)/configure:
	cd $(HTOP_DIR) && ./autogen.sh

# After configure, tweak config.h so htop's ProvideCurses.h /
# ProvideTerm.h land on plain <curses.h> / <term.h> (ours, via AM_CPPFLAGS)
# instead of the system ncursesw/ncurses variants. HAVE_LIBNCURSESW stays
# on so htop keeps its wide-character code paths.
$(HTOP_DIR)/Makefile: $(HTOP_DIR)/configure
	cd $(HTOP_DIR) && ./configure
	sed -i \
	    -e '/^#define HAVE_NCURSESW_CURSES_H /d' \
	    -e '/^#define HAVE_NCURSESW_TERM_H /d' \
	    -e '/^#define HAVE_NCURSES_CURSES_H /d' \
	    -e '/^#define HAVE_NCURSES_TERM_H /d' \
	    -e '/^#define HAVE_NCURSES_NCURSES_H /d' \
	    -e '/^#define HAVE_NCURSES_H /d' \
	    -e 's|^/\* #undef HAVE_CURSES_H \*/|#define HAVE_CURSES_H 1|' \
	    $(HTOP_DIR)/config.h

$(BINDIR)/htop: $(CURSES_TOBJ) $(HTOP_DIR)/Makefile | $(BINDIR)
	cd $(HTOP_DIR) && $(MAKE) \
	    LIBS='$(HTOP_LIBS_OVERRIDE)' \
	    AM_CPPFLAGS='$(HTOP_PA_CPPFLAGS)' htop
	mv $(HTOP_DIR)/htop $@

clean:
	rm -f $(CURSES_TOBJ) $(CURSES_GOBJ) $(BIN_EXES) $(BIN_ADP) \
	      $(BINDIR)/gomoku $(BINDIR)/gomokug
	@if [ -f $(HTOP_DIR)/Makefile ]; then \
	    $(MAKE) -C $(HTOP_DIR) clean > /dev/null 2>&1 || true; \
	fi
