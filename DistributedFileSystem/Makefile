CC     := gcc
CFLAGS := -Wall -Werror 

SRCS   := client.c \
	server.c 

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

.PHONY: all

all: server client.c libmfs.so mkfs.c
	gcc -Wall -Werror mkfs.c -o mkfs
	export LD_LIBRARY_PATH=${CURDIR}

libmfs.so: libmfs.o
	gcc -shared -Wl,-soname,libmfs.so -o libmfs.so libmfs.o udp.c -lc

libmfs.o: libmfs.c
	gcc -fPIC -g -c libmfs.c

server: server.c udp.c mfs.h ufs.h udp.h
	gcc -Wall -Werror server.c udp.c -o server

clean:
	rm -f *.o
	rm -f mkfs
	rm -f *.so
	rm -f *.img

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<