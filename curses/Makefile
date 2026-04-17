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

TLIBS   = $(CURSES_OBJ) $(ROOT)/stub/keeper.o $(ROOT)/lib/petit_ami_term.so \
          $(ROOT)/linux/sound.o $(ROOT)/linux/fluidsynthplug.o \
          $(ROOT)/linux/dumpsynthplug.o \
          -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto -lbsd

GLIBS   = $(CURSES_OBJ) $(ROOT)/stub/keeper.o $(ROOT)/lib/petit_ami_graph.so \
          $(ROOT)/linux/sound.o $(ROOT)/linux/fluidsynthplug.o \
          $(ROOT)/linux/dumpsynthplug.o \
          -lasound -lfluidsynth -lm -lpthread -lssl -lcrypto \
          -lX11 -lfreetype -lfontconfig -lbsd

GAMES   = snake rain worm worms

all: $(CURSES_OBJ) $(foreach g,$(GAMES),$(g)_term $(g)_gfx)

snake: snake_term snake_gfx
rain: rain_term rain_gfx
worm: worm_term worm_gfx
worms: worms_term worms_gfx

$(CURSES_OBJ): $(ROOT)/apis/curses.c $(ROOT)/apis/curses.h
	$(CC) $(AMIFLAGS) -c $(ROOT)/apis/curses.c -o $(CURSES_OBJ)

# Snake
SNAKE_SRC   = $(ROOT)/BSDGames/snake/snake/snake.c
SNAKE_FLAGS = -I$(ROOT)/BSDGames/snake/snake

snake_term: $(CURSES_OBJ) $(SNAKE_SRC)
	$(CC) $(CFLAGS) $(SNAKE_FLAGS) -o $@ $(SNAKE_SRC) $(TLIBS) -lm

snake_gfx: $(CURSES_OBJ) $(SNAKE_SRC)
	$(CC) $(CFLAGS) $(SNAKE_FLAGS) -o $@ $(SNAKE_SRC) $(GLIBS) -lm

# Rain
RAIN_SRC = $(ROOT)/BSDGames/rain/rain.c

rain_term: $(CURSES_OBJ) $(RAIN_SRC)
	$(CC) $(CFLAGS) -o $@ $(RAIN_SRC) $(TLIBS)

rain_gfx: $(CURSES_OBJ) $(RAIN_SRC)
	$(CC) $(CFLAGS) -o $@ $(RAIN_SRC) $(GLIBS)

# Worm
WORM_SRC = $(ROOT)/BSDGames/worm/worm.c

worm_term: $(CURSES_OBJ) $(WORM_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORM_SRC) $(TLIBS)

worm_gfx: $(CURSES_OBJ) $(WORM_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORM_SRC) $(GLIBS)

# Worms
WORMS_SRC = $(ROOT)/BSDGames/worms/worms.c

worms_term: $(CURSES_OBJ) $(WORMS_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORMS_SRC) $(TLIBS)

worms_gfx: $(CURSES_OBJ) $(WORMS_SRC)
	$(CC) $(CFLAGS) -o $@ $(WORMS_SRC) $(GLIBS)

clean:
	rm -f $(CURSES_OBJ) *_term *_gfx

.PHONY: all clean snake rain worm worms
