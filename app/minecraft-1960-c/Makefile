CC      ?= cc
CFLAGS  ?= -O3 -Wall -Wextra -std=c89 -pedantic
OBJS     = main.o game.o world.o craft.o render.o display_ref.o
HAVE_X11 := $(shell pkg-config --exists x11 2>/dev/null && echo 1)

all: mc mctest $(if $(HAVE_X11),mcx)

mc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# window driver: real pixels in an X11 window
mcx: main.o game.o world.o craft.o render.o display_x11.o
	$(CC) $(CFLAGS) -o $@ main.o game.o world.o craft.o render.o display_x11.o \
	  $(shell pkg-config --libs x11)

display_x11.o: display_x11.c mc.h display.h
	$(CC) $(CFLAGS) $(shell pkg-config --cflags x11) -c $< -o $@

# test helper: inject key presses into the game window
xsendkey: tools/xsendkey.c
	$(CC) $(CFLAGS) -o $@ tools/xsendkey.c $(shell pkg-config --libs x11)

# headless verification build (scripted input + state report)
mctest: main.o game.o world.o craft.o render.o display_test.o
	$(CC) $(CFLAGS) -o $@ main.o game.o world.o craft.o render.o display_test.o

%.o: %.c mc.h display.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f mc mctest mcx $(OBJS) display_test.o display_x11.o

.PHONY: clean all
