CC      = gcc
CFLAGS  = -Wall -Wextra -g

OBJS = server2.o awale.o game.o commands.o

all: server

server: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c types.h awale.h server2.h game.h commands.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o server

.PHONY: all test clean