INCLUDE_DIRS = 
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 $(INCLUDE_DIRS) $(CDEFS)
LIBS= -lpthread -lrt

HFILES= 
CFILES= heap_mq.c

SRCS= ${HFILES} ${CFILES}

OBJS= ${CFILES:.c=.o}

all:	heap_mq

clean:
	-rm -f *.o *.d
	-rm -f perfmon heap_mq 

heap_mq: heap_mq.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) $(LIBS)


depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
