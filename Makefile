CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS=-lrt -pthread


all: roller.o
	gcc $(CFLAGS) $< -o roller $(LFLAGS)

roller.o: roller.c
	gcc $(CFLAGS) -c $< -o roller.o

###################################################################
zip:
	zip xbarvi00.zip roller.c Makefile
clean:
	rm -rf *.o roller
