CC := gcc
AR := ar

CFLAGS = -Wall -g 
LDLIBS = -Wl,-rpath 

all: test

%.o: %.c json.h 
	$(CC) $(CFLAGS) -c $<

SRCS=test.c json.c
OBJS=$(SRCS:.c=.o)

test: ${OBJS}
	$(CC) $(CFLAGS) ${OBJS} -o $@ $(LDLIBS)

clean:
	rm -f *.o $(UTILS)
