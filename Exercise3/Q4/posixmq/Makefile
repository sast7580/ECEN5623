INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lpthread -lrt

HFILES= 
CFILES= posix_mq.c

SRCS= ${HFILES} ${CFILES}

OBJS= ${CFILES:.c=.o}

all:	posix_mq

clean:
	-rm -f *.o *.d
	-rm -f perfmon posix_mq 

posix_mq: posix_mq.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
