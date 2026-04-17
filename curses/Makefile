#
# Makefile for BSD curses games on Petit-Ami
#
# Builds each game in both terminal and graphics versions using the
# curses compatibility layer in apis/curses.c.
#
# Usage: make          (builds all)
#        make snake     (builds snake_term and snake_gfx)
#        make clean
#

ROOT    = ..
CC      = gcc

# flags for building the curses adapter (needs Ami stdio interception)
AMIFLAGS = -g3 -I$(ROOT)/apis -I$(ROOT)/include -I$(ROOT)/libc \
           -I/usr/include/freetype2 -I/usr/include/libpng16 -DSTDIO_BYPASS

# flags for building game source (standard libc, no Ami stdio override)
CFLAGS  = -g3 -I$(ROOT)/apis -D'__COPYRIGHT(x)=' -D'__RCSID(x)='

CURSES_OBJ = $(ROOT)/apis/curses.o

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

TLIBS   = $(CURSES_OBJ) $(ROOT)/stub/keeper.o $(TOBJ) \
          $(ROOT)/linux/sound.o $(ROOT)/linux/fluidsynthplug.o \
          $(ROOT)/linux/dumpsynthplug.o \
          -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto -lbsd -lstdc++

GLIBS   = $(CURSES_OBJ) $(ROOT)/stub/keeper.o $(GOBJ) \
          $(ROOT)/linux/sound.o $(ROOT)/linux/fluidsynthplug.o \
          $(ROOT)/linux/dumpsynthplug.o \
          -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto \
          -lX11 -lXext -lfreetype -lfontconfig -lbsd -lstdc++

GAMES   = snake rain worm worms canfield

all: $(CURSES_OBJ) $(foreach g,$(GAMES),$(g)_term $(g)_gfx)

snake: snake_term snake_gfx
rain: rain_term rain_gfx
worm: worm_term worm_gfx
worms: worms_term worms_gfx
canfield: canfield_term canfield_gfx
gomoku: gomoku_term gomoku_gfx

$(CURSES_OBJ): $(ROOT)/apis/curses.c $(ROOT)/apis/curses.h
	$(CC) $(AMIFLAGS) -c $(ROOT)/apis/curses.c -o $(CURSES_OBJ)

# Snake
SNAKE_SRC   = BSDGames/snake/snake/snake.c
SNAKE_FLAGS = -IBSDGames/snake/snake

snake_term: $(CURSES_OBJ) $(SNAKE_SRC)
	$(CC) $(CFLAGS) $(SNAKE_FLAGS) -o $@ $(SNAKE_SRC) $(TLIBS) -lm

snake_gfx: $(CURSES_OBJ) $(SNAKE_SRC)
	$(CC) $(CFLAGS) $(SNAKE_FLAGS) -o $@ $(SNAKE_SRC) $(GLIBS) -lm

# Rain
RAIN_SRC = BSDGames/rain/rain.c

rain_term: $(CURSES_OBJ) $(RAIN_SRC)
	$(CC) $(CFLAGS) -o $@ $(RAIN_SRC) $(TLIBS)

rain_gfx: $(CURSES_OBJ) $(RAIN_SRC)
	$(CC) $(CFLAGS) -o $@ $(RAIN_SRC) $(GLIBS)

# Worm
WORM_SRC = BSDGames/worm/worm.c

worm_term: $(CURSES_OBJ) $(WORM_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORM_SRC) $(TLIBS)

worm_gfx: $(CURSES_OBJ) $(WORM_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORM_SRC) $(GLIBS)

# Worms
WORMS_SRC = BSDGames/worms/worms.c

worms_term: $(CURSES_OBJ) $(WORMS_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORMS_SRC) $(TLIBS)

worms_gfx: $(CURSES_OBJ) $(WORMS_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORMS_SRC) $(GLIBS)

# Canfield
CANFIELD_SRC   = BSDGames/canfield/canfield/canfield.c
CANFIELD_FLAGS = -IBSDGames/canfield/canfield

canfield_term: $(CURSES_OBJ) $(CANFIELD_SRC)
	$(CC) $(CFLAGS) $(CANFIELD_FLAGS) -o $@ $(CANFIELD_SRC) $(TLIBS)

canfield_gfx: $(CURSES_OBJ) $(CANFIELD_SRC)
	$(CC) $(CFLAGS) $(CANFIELD_FLAGS) -o $@ $(CANFIELD_SRC) $(GLIBS)

# Gomoku
GOMOKU_SRC   = BSDGames/gomoku/main.c BSDGames/gomoku/bdisp.c \
               BSDGames/gomoku/stoc.c BSDGames/gomoku/makemove.c \
               BSDGames/gomoku/pickmove.c BSDGames/gomoku/bdinit.c
GOMOKU_FLAGS = -IBSDGames/gomoku -I$(ROOT)/apis/bsdcompat -Dgetline=gomoku_getline

gomoku_term: $(CURSES_OBJ) $(GOMOKU_SRC)
	$(CC) $(CFLAGS) $(GOMOKU_FLAGS) -o $@ $(GOMOKU_SRC) $(TLIBS)

gomoku_gfx: $(CURSES_OBJ) $(GOMOKU_SRC)
	$(CC) $(CFLAGS) $(GOMOKU_FLAGS) -o $@ $(GOMOKU_SRC) $(GLIBS)

clean:
	rm -f $(CURSES_OBJ) *_term *_gfx

.PHONY: all clean snake rain worm worms
