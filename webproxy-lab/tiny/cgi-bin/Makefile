CC = gcc
#CFLAGS = -O2 -Wall -I ..
CFLAGS = -O0 -Wall -I .. -g

all: adder

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

clean:
	rm -f adder *~
