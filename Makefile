LDFLAGS=-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo -L/opt/homebrew/lib -lSDL2
CFLAGS=-O2 -Wall -I/opt/homebrew/include/SDL2 -D_THREAD_SAFE

CC=gcc

all:	esc

esc:	console.o tracker.o chip.o p1xl.o lft/lft.o bv/bv.o actions.o blip_buf.o
		${CC} -o $@ $^ ${LDFLAGS}

%.o:	%.c tracker.h Makefile
		${CC} -c ${CFLAGS} $< -o $@

clean:
		rm *.o bv/*.o lft/*.o esc
